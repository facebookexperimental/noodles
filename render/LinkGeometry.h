// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_LINK_GEOMETRY_H
#define NOODLES_RENDER_LINK_GEOMETRY_H

#include <tuple>
#include <vector>
#include "core/Math.h"
#include "core/api.h"

namespace noodles {

/// Link geometry utilities for accelerated link rendering and hit testing
class NOODLES_API LinkGeometry {
 public:
  static int findLinkUnderCursor(
      const Vec2d& cursorPos,
      const std::vector<std::pair<Vec2d, Vec2d>>& linkEndpoints,
      double worldTolerance);

  static bool linkIntersectsBounds(const Vec2d& start, const Vec2d& end, const Range2d& bounds);

  static std::vector<int> findLinksInBounds(
      const std::vector<std::pair<Vec2d, Vec2d>>& linkEndpoints,
      const Range2d& bounds);

  /// Calculate LOD level based on Manhattan distance
  ///
  /// Args:
  ///     manhattanLength: Manhattan distance between link endpoints
  ///     diffX: linkSampleRate / zoom
  ///     lodThresholds: Vector of (threshold, level) pairs, sorted descending by threshold
  ///
  /// Returns:
  ///     LOD level (number of samples for the reference curve)
  static int calculateLOD(
      double manhattanLength,
      double diffX,
      const std::vector<std::pair<int, int>>& lodThresholds);

  /// Generate reference curve vertices for a given sample count
  ///
  /// Creates curve with cosine-squared falloff profile.
  /// Returns flat float array: [pos.x, pos.y, prev.x, prev.y, next.x, next.y, dir, ...] *
  /// numSamples * 2
  ///
  /// Args:
  ///     numSamples: Number of samples along the curve
  ///
  /// Returns:
  ///     Vector of floats (7 floats per vertex, 2 vertices per sample)
  static std::vector<float> generateReferenceCurve(int numSamples);

  /// Sample the shaped link "noodle" curve from start to end into a polyline of
  /// `numSamples` points (must be >= 2; returns empty otherwise).
  ///
  /// Reproduces the GPU vertex shader (assets/shaders/link_poly_vert.glsl)
  /// shaping on the CPU so that hit-testing matches what is rendered: either the
  /// forward<->backward bezier6 blend, or -- when `verticalEndTangent` is true --
  /// the prim-target cubic that enters the target vertically. The caller is
  /// responsible for applying any prim-target arrow inset to `end` beforehand
  /// (see kLinkPrimTargetArrow* in LinkCurveParams.h). Points are in world space.
  static std::vector<Vec2d>
  sampleLinkCurve(const Vec2d& start, const Vec2d& end, int numSamples, bool verticalEndTangent);

  /// Curvature-adaptive variant of `sampleLinkCurve`: recursively subdivides
  /// where the curve bends -- many points on the backward S-bow, few on a
  /// near-straight link -- until every chord lies within `flatnessTolerance`
  /// world units of the curve. Endpoints are exact, same shaping as
  /// `sampleLinkCurve`. Zoom-independent, so the result is intended to be cached
  /// and reused across pan/zoom until the link's geometry changes.
  static std::vector<Vec2d> sampleLinkCurveAdaptive(
      const Vec2d& start,
      const Vec2d& end,
      bool verticalEndTangent,
      double flatnessTolerance);

  /// Conservative axis-aligned bounds of the shaped link curve, including the
  /// backward S-bow that extends past the straight start/end box. Computed from
  /// `numSamples` curve samples (>= 2; otherwise the straight endpoint AABB).
  ///
  /// Intended as the broad-phase key in the spatial index: callers should pass
  /// the raw (un-inset) `end` so the bounds are zoom-independent and stay valid
  /// without a rebuild on zoom (the prim-target arrow inset only shrinks the
  /// curve, so omitting it keeps the bounds conservative).
  static Range2d computeLinkCurveBounds(
      const Vec2d& start,
      const Vec2d& end,
      int numSamples,
      bool verticalEndTangent);

 private:
  static double
  distanceToSegmentSquared(const Vec2d& point, const Vec2d& segStart, const Vec2d& segEnd);

  static bool lineSegmentIntersectsEdge(
      const Vec2d& p1,
      const Vec2d& p2,
      const Vec2d& edgeP1,
      const Vec2d& edgeP2);
};

} // namespace noodles

#endif // NOODLES_RENDER_LINK_GEOMETRY_H
