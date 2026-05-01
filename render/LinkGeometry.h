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
