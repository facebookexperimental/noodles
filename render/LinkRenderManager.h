// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_LINK_RENDER_MANAGER_H
#define NOODLES_RENDER_LINK_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/api.h"
#include "core/pragmas.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API LinkInstanceCache {
  int generation = 0;
  int hits = 0;
  int misses = 0;
  int regenerations = 0;

  void invalidate() {
    generation++;
  }
  void clear() {
    generation = 0;
    hits = 0;
    misses = 0;
    regenerations = 0;
  }
};

class NOODLES_API LinkRenderManager {
 public:
  LinkRenderManager();
  ~LinkRenderManager();

  LinkRenderManager(const LinkRenderManager&) = delete;
  LinkRenderManager& operator=(const LinkRenderManager&) = delete;
  LinkRenderManager(LinkRenderManager&&) = delete;
  LinkRenderManager& operator=(LinkRenderManager&&) = delete;

  void initialize(ShaderLibrary* shaders);

  void renderLinks(
      const std::vector<LinkData>& links,
      const float* projection4x4,
      float thickness,
      float zoom,
      float panX,
      float panY,
      float viewportWidth,
      float viewportHeight,
      float dimming,
      bool drawSelected,
      const float* baseColor = nullptr,
      const float* selectedColor = nullptr,
      const float* hoveredColor = nullptr,
      const float* highlightedColor = nullptr);

  void invalidateCache();
  int getCacheGeneration() const {
    return instanceCache_.generation;
  }
  int getCacheHits() const {
    return instanceCache_.hits;
  }
  int getCacheMisses() const {
    return instanceCache_.misses;
  }
  int getCacheRegenerations() const {
    return instanceCache_.regenerations;
  }

  void cleanup();

 private:
  static constexpr std::array<int, 6> LOD_LEVELS = {5, 10, 20, 40, 80, 160};
  static constexpr int NUM_LOD_LEVELS = 6;

  struct LodThreshold {
    int segmentThreshold;
    int sampleCount;
  };
  static constexpr std::array<LodThreshold, 5> LOD_THRESHOLDS = {
      {{80, 160}, {40, 80}, {20, 40}, {10, 20}, {5, 10}}};

  GLuint ensureReferenceCurveVbo(int numSamples);
  int getLodLevel(int numSegments) const;

  std::unordered_map<int, GLuint> refCurveVbos_;
  std::unordered_map<int, GLuint> instanceVbos_;
  GLuint vao_ = 0;
  ShaderLibrary* shaders_ = nullptr;
  bool initialized_ = false;
  LinkInstanceCache instanceCache_;
};
NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_LINK_RENDER_MANAGER_H
