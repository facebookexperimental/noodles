// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#define _USE_MATH_DEFINES
#include "render/LinkGeometry.h"
#include <algorithm>
#include <cmath>

namespace noodles {

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

} // namespace noodles
