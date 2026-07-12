// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#define _USE_MATH_DEFINES
#include "render/LinkGeometry.h"
#include <algorithm>
#include <cmath>
#include "render/LinkCurveParams.h"

namespace noodles {

namespace {

// Scale a vector by a scalar (Vec2d has +/- but no scalar multiply).
Vec2d scaled(const Vec2d& v, double s) {
  return {v[0] * s, v[1] * s};
}

// Cubic Bezier; mirrors cubicBezier() in link_poly_vert.glsl.
Vec2d cubicBezier(const Vec2d& p0, const Vec2d& p1, const Vec2d& p2, const Vec2d& p3, double t) {
  const double u = 1.0 - t;
  const double tt = t * t;
  const double uu = u * u;
  const double uuu = uu * u;
  const double ttt = tt * t;
  return scaled(p0, uuu) + scaled(p1, 3.0 * uu * t) + scaled(p2, 3.0 * u * tt) + scaled(p3, ttt);
}

// Two cubics stitched at the midpoint of p0/p5; mirrors bezier6() in the shader.
Vec2d bezier6(
    const Vec2d& p0,
    const Vec2d& p1,
    const Vec2d& p2,
    const Vec2d& p3,
    const Vec2d& p4,
    const Vec2d& p5,
    double t) {
  const Vec2d mid{(p0[0] + p5[0]) * 0.5, (p0[1] + p5[1]) * 0.5};
  if (t < 0.5) {
    return cubicBezier(p0, p1, p2, mid, t * 2.0);
  }
  return cubicBezier(mid, p3, p4, p5, (t - 0.5) * 2.0);
}

// GLSL smoothstep(edge0, edge1, x).
double smoothstep(double edge0, double edge1, double x) {
  const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

// Reference profile fed to the shader as `refPosition`: linear in x, cosine-
// squared falloff in y. Same as generateReferenceCurve()'s curveFunc.
Vec2d referenceCurvePoint(double t) {
  const double tClamped = std::clamp(t, 0.0, 1.0);
  const double cosT = std::cos(tClamped * M_PI / 2.0);
  return {t, 1.0 - cosT * cosT};
}

// Prim-target end-tangent handle length: clamp(|delta| * factor, min, max).
double endHandle(double delta) {
  return std::clamp(std::abs(delta) * kLinkEndHandleFactor, kLinkEndHandleMin, kLinkEndHandleMax);
}

// Precomputed shaping for one link curve, evaluable at any parameter t in
// [0, 1]. Shared by the uniform and adaptive samplers so the shaping math lives
// in a single place. Mirrors link_poly_vert.glsl (constants in LinkCurveParams.h).
struct LinkCurveShape {
  bool vertical = false;
  double sx = 0.0;
  double sy = 0.0;
  double scaleX = 0.0;
  double scaleY = 0.0;
  double mx = 0.0;
  Vec2d p0, p1, p2, p3, p4, p5;

  Vec2d evalAt(double t) const {
    if (vertical) {
      return cubicBezier(p0, p1, p2, p3, t);
    }
    const Vec2d rp = referenceCurvePoint(t);
    const double baseX = sx + rp[0] * scaleX;
    const double baseY = sy + rp[1] * scaleY;
    if (mx > 0.0) {
      const Vec2d b = bezier6(p0, p1, p2, p3, p4, p5, rp[0]);
      return {baseX + mx * (b[0] - baseX), baseY + mx * (b[1] - baseY)};
    }
    return {baseX, baseY};
  }
};

LinkCurveShape buildLinkCurveShape(const Vec2d& start, const Vec2d& end, bool verticalEndTangent) {
  LinkCurveShape shape;
  shape.vertical = verticalEndTangent;
  const double sx = start[0];
  const double sy = start[1];
  const double ex = end[0];
  const double ey = end[1];
  shape.sx = sx;
  shape.sy = sy;
  shape.scaleX = ex - sx;
  shape.scaleY = ey - sy;

  if (verticalEndTangent) {
    // Whole-prim relationship target: a single cubic that leaves the source
    // horizontally and enters the target vertically. Mirrors the
    // verticalEndTangent branch of link_poly_vert.glsl.
    const double horizontalDir = std::abs(shape.scaleX) > kLinkHorizontalDirEpsilon
        ? (shape.scaleX > 0.0 ? 1.0 : -1.0)
        : 1.0;
    shape.p0 = {sx, sy};
    shape.p1 = {sx + horizontalDir * endHandle(shape.scaleX), sy};
    shape.p2 = {ex, ey - endHandle(shape.scaleY)};
    shape.p3 = {ex, ey};
    return shape;
  }

  // Forward/backward blend. The backward S-curve control points push the handles
  // out by `d` horizontally and by `*yOff` vertically; `mx` smoothly blends from
  // the straight/forward reference curve (mx==0) to the backward bezier6 (mx==1)
  // as the link points increasingly right-to-left. Mirrors the else branch of
  // link_poly_vert.glsl.
  const double d = std::clamp(
      (sx - ex) * kLinkBackwardHandleFactor, kLinkBackwardHandleMin, kLinkTransitionSpan);
  const double midY = (sy + ey) * 0.5;
  const double sgn = ey > sy ? 1.0 : (ey < sy ? -1.0 : 0.0);
  const double p2yOff = std::max((midY - sy) * kLinkMidSharpness, sgn * kLinkBackwardYOffMin);
  const double p3yOff = std::max((ey - midY) * kLinkMidSharpness, sgn * kLinkBackwardYOffMin);
  shape.p0 = {sx, sy};
  shape.p1 = {sx + d, sy};
  shape.p2 = {sx + d, sy + p2yOff};
  shape.p3 = {ex - d, ey - p3yOff};
  shape.p4 = {ex - d, ey};
  shape.p5 = {ex, ey};
  const double tOffsetFactor = std::min(std::abs(sy - ey) / kLinkYMinDist, 1.0);
  shape.mx = smoothstep(
      0.0,
      1.0,
      std::clamp(
          (sx - ex + (kLinkTransitionSpan + kLinkTransitionOffset) * tOffsetFactor) *
              kLinkTransitionMxFactor,
          0.0,
          kLinkTransitionSpan) /
          kLinkTransitionSpan);
  return shape;
}

// Recursion depth bounds for adaptive flattening. The MIN depth forces a few
// initial splits: a backward link is a symmetric S whose midpoint sits exactly
// on the chord midpoint, so a single midpoint-deviation test reads zero and
// would wrongly call the bow "flat" -- forcing splits exposes the quarter-points
// where the S actually deviates. The MAX depth bounds the worst-case point count.
constexpr int kLinkAdaptiveMinDepth = 2;
constexpr int kLinkAdaptiveMaxDepth = 16;

// Append interior curve points strictly between t0 and t1, in order. Subdivides
// while the curve midpoint deviates from the chord c0->c1 by more than
// `flatnessSq` (squared world units), but always to at least `minDepth` levels.
// The endpoints are emitted by the caller.
void subdivideLinkCurve(
    const LinkCurveShape& shape,
    double t0,
    double t1,
    const Vec2d& c0,
    const Vec2d& c1,
    double flatnessSq,
    int depth,
    int minDepth,
    std::vector<Vec2d>& out) {
  if (depth <= 0) {
    return;
  }
  const double tm = (t0 + t1) * 0.5;
  const Vec2d cm = shape.evalAt(tm);
  const double dx = cm[0] - (c0[0] + c1[0]) * 0.5;
  const double dy = cm[1] - (c0[1] + c1[1]) * 0.5;
  if (minDepth <= 0 && dx * dx + dy * dy <= flatnessSq) {
    return; // Flat enough and past the minimum depth: the chord c0->c1 suffices.
  }
  subdivideLinkCurve(shape, t0, tm, c0, cm, flatnessSq, depth - 1, minDepth - 1, out);
  out.push_back(cm);
  subdivideLinkCurve(shape, tm, t1, cm, c1, flatnessSq, depth - 1, minDepth - 1, out);
}

} // namespace

double LinkGeometry::distanceToSegmentSquared(
    const Vec2d& point,
    const Vec2d& segStart,
    const Vec2d& segEnd) {
  // Vector from start to end
  double dx = segEnd[0] - segStart[0];
  double dy = segEnd[1] - segStart[1];
  double lengthSquared = dx * dx + dy * dy;

  if (lengthSquared < 0.0001) {
    // Degenerate segment (start == end), return distance to point
    double px = point[0] - segStart[0];
    double py = point[1] - segStart[1];
    return px * px + py * py;
  }

  // Project point onto the line segment
  // t = dot(point - start, end - start) / |end - start|^2
  double t = ((point[0] - segStart[0]) * dx + (point[1] - segStart[1]) * dy) / lengthSquared;

  // Clamp t to [0, 1] to stay on segment
  t = std::clamp(t, 0.0, 1.0);

  // Find projection point
  double projX = segStart[0] + t * dx;
  double projY = segStart[1] + t * dy;

  // Return squared distance to projection
  double distX = point[0] - projX;
  double distY = point[1] - projY;
  return distX * distX + distY * distY;
}

bool LinkGeometry::lineSegmentIntersectsEdge(
    const Vec2d& p1,
    const Vec2d& p2,
    const Vec2d& edgeP1,
    const Vec2d& edgeP2) {
  // Using parametric line segment intersection
  double x1 = p1[0], y1 = p1[1];
  double x2 = p2[0], y2 = p2[1];
  double x3 = edgeP1[0], y3 = edgeP1[1];
  double x4 = edgeP2[0], y4 = edgeP2[1];

  double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
  if (std::abs(denom) < 1e-10) {
    return false; // Parallel lines
  }

  double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
  double u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

  return t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0;
}

int LinkGeometry::findLinkUnderCursor(
    const Vec2d& cursorPos,
    const std::vector<std::pair<Vec2d, Vec2d>>& linkEndpoints,
    double worldTolerance) {
  //
  // Uses optimized distance-to-line-segment calculations with early-out
  // bounding box tests for O(n) but fast linear search.
  //

  double worldToleranceSquared = worldTolerance * worldTolerance;
  int closestIndex = -1;
  double closestDistSquared = std::numeric_limits<double>::infinity();

  for (size_t i = 0; i < linkEndpoints.size(); ++i) {
    const Vec2d& start = linkEndpoints[i].first;
    const Vec2d& end = linkEndpoints[i].second;

    // Fast bounding box rejection test
    double minX = std::min(start[0], end[0]) - worldTolerance;
    double maxX = std::max(start[0], end[0]) + worldTolerance;
    double minY = std::min(start[1], end[1]) - worldTolerance;
    double maxY = std::max(start[1], end[1]) + worldTolerance;

    if (cursorPos[0] < minX || cursorPos[0] > maxX || cursorPos[1] < minY || cursorPos[1] > maxY) {
      continue; // Outside bounding box
    }

    // Calculate squared distance to line segment
    double distSquared = distanceToSegmentSquared(cursorPos, start, end);

    if (distSquared < worldToleranceSquared && distSquared < closestDistSquared) {
      closestDistSquared = distSquared;
      closestIndex = static_cast<int>(i);
    }
  }

  return closestIndex;
}

bool LinkGeometry::linkIntersectsBounds(
    const Vec2d& start,
    const Vec2d& end,
    const Range2d& bounds) {
  Vec2d boundsMin = bounds.GetMin();
  Vec2d boundsMax = bounds.GetMax();
  double minX = boundsMin[0], minY = boundsMin[1];
  double maxX = boundsMax[0], maxY = boundsMax[1];

  // Check if either endpoint is inside the bounds
  if (start[0] >= minX && start[0] <= maxX && start[1] >= minY && start[1] <= maxY) {
    return true;
  }
  if (end[0] >= minX && end[0] <= maxX && end[1] >= minY && end[1] <= maxY) {
    return true;
  }

  // Define the 4 corners of the rectangle
  Vec2d topLeft(minX, minY);
  Vec2d topRight(maxX, minY);
  Vec2d bottomLeft(minX, maxY);
  Vec2d bottomRight(maxX, maxY);

  // Check intersection with each of the 4 edges
  // Top edge
  if (lineSegmentIntersectsEdge(start, end, topLeft, topRight)) {
    return true;
  }
  // Right edge
  if (lineSegmentIntersectsEdge(start, end, topRight, bottomRight)) {
    return true;
  }
  // Bottom edge
  if (lineSegmentIntersectsEdge(start, end, bottomRight, bottomLeft)) {
    return true;
  }
  // Left edge
  if (lineSegmentIntersectsEdge(start, end, bottomLeft, topLeft)) {
    return true;
  }

  return false;
}

std::vector<int> LinkGeometry::findLinksInBounds(
    const std::vector<std::pair<Vec2d, Vec2d>>& linkEndpoints,
    const Range2d& bounds) {
  std::vector<int> result;
  result.reserve(linkEndpoints.size() / 4); // Estimate ~25% will match

  for (size_t i = 0; i < linkEndpoints.size(); ++i) {
    if (linkIntersectsBounds(linkEndpoints[i].first, linkEndpoints[i].second, bounds)) {
      result.push_back(static_cast<int>(i));
    }
  }

  return result;
}

int LinkGeometry::calculateLOD(
    double manhattanLength,
    double diffX,
    const std::vector<std::pair<int, int>>& lodThresholds) {
  if (diffX <= 0.0) {
    // Return highest LOD if diffX is invalid
    return lodThresholds.empty() ? 160 : lodThresholds.front().second;
  }

  int numSegments = static_cast<int>(manhattanLength / diffX);

  for (const auto& threshold : lodThresholds) {
    if (numSegments > threshold.first) {
      return threshold.second;
    }
  }

  return 5; // Minimum LOD level
}

std::vector<float> LinkGeometry::generateReferenceCurve(int numSamples) {
  if (numSamples < 2) {
    return std::vector<float>();
  }

  // Reserve space: 7 floats per vertex, 2 vertices per sample
  std::vector<float> vertices;
  vertices.reserve(numSamples * 2 * 7);

  double step = 1.0 / (numSamples - 1);

  // Curve function: cosine-squared falloff
  auto curveFunc = [](double t) -> Vec2f {
    double x = t;
    double tClamped = std::clamp(t, 0.0, 1.0);
    double cosT = std::cos(tClamped * M_PI / 2.0);
    double y = 1.0 - cosT * cosT;
    return Vec2f(static_cast<float>(x), static_cast<float>(y));
  };

  for (int i = 0; i < numSamples; ++i) {
    double t = i * step;
    double prevT = t - step;
    double nextT = t + step;

    Vec2f pos = curveFunc(t);
    Vec2f prevPos = curveFunc(prevT);
    Vec2f nextPos = curveFunc(nextT);

    // Vertex on one side (dir = -1.0)
    vertices.push_back(pos[0]);
    vertices.push_back(pos[1]);
    vertices.push_back(prevPos[0]);
    vertices.push_back(prevPos[1]);
    vertices.push_back(nextPos[0]);
    vertices.push_back(nextPos[1]);
    vertices.push_back(-1.0f);

    // Vertex on other side (dir = +1.0)
    vertices.push_back(pos[0]);
    vertices.push_back(pos[1]);
    vertices.push_back(nextPos[0]);
    vertices.push_back(nextPos[1]);
    vertices.push_back(prevPos[0]);
    vertices.push_back(prevPos[1]);
    vertices.push_back(1.0f);
  }

  return vertices;
}

std::vector<Vec2d> LinkGeometry::sampleLinkCurve(
    const Vec2d& start,
    const Vec2d& end,
    int numSamples,
    bool verticalEndTangent) {
  std::vector<Vec2d> points;
  if (numSamples < 2) {
    return points;
  }
  points.reserve(numSamples);
  const LinkCurveShape shape = buildLinkCurveShape(start, end, verticalEndTangent);
  for (int i = 0; i < numSamples; ++i) {
    points.push_back(shape.evalAt(static_cast<double>(i) / (numSamples - 1)));
  }
  return points;
}

std::vector<Vec2d> LinkGeometry::sampleLinkCurveAdaptive(
    const Vec2d& start,
    const Vec2d& end,
    bool verticalEndTangent,
    double flatnessTolerance) {
  const LinkCurveShape shape = buildLinkCurveShape(start, end, verticalEndTangent);
  const Vec2d c0 = shape.evalAt(0.0);
  const Vec2d c1 = shape.evalAt(1.0);
  std::vector<Vec2d> points;
  points.push_back(c0);
  const double flatnessSq = flatnessTolerance > 0.0 ? flatnessTolerance * flatnessTolerance : 0.0;
  subdivideLinkCurve(
      shape, 0.0, 1.0, c0, c1, flatnessSq, kLinkAdaptiveMaxDepth, kLinkAdaptiveMinDepth, points);
  points.push_back(c1);
  return points;
}

Range2d LinkGeometry::computeLinkCurveBounds(
    const Vec2d& start,
    const Vec2d& end,
    int numSamples,
    bool verticalEndTangent) {
  const std::vector<Vec2d> pts = sampleLinkCurve(start, end, numSamples, verticalEndTangent);
  if (pts.empty()) {
    // Degenerate sample count: fall back to the straight endpoint AABB.
    return Range2d(
        Vec2d(std::min(start[0], end[0]), std::min(start[1], end[1])),
        Vec2d(std::max(start[0], end[0]), std::max(start[1], end[1])));
  }
  double minX = pts[0][0];
  double maxX = pts[0][0];
  double minY = pts[0][1];
  double maxY = pts[0][1];
  for (const Vec2d& p : pts) {
    minX = std::min(minX, p[0]);
    maxX = std::max(maxX, p[0]);
    minY = std::min(minY, p[1]);
    maxY = std::max(maxY, p[1]);
  }
  return Range2d(Vec2d(minX, minY), Vec2d(maxX, maxY));
}

} // namespace noodles
