// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_LINK_RENDER_MANAGER_H
#define NOODLES_RENDER_LINK_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/RenderConfig.h"
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
  int hits = 0;
  int misses = 0;

  void clear() {
    hits = 0;
    misses = 0;
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
      const RenderConfig& config,
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
      const float* highlightedColor = nullptr,
      // Distinguishes independent call sites that share this manager within a
      // frame (e.g. regular vs relationship links). Each (cacheKey, drawSelected,
      // LOD) keeps its own instance VBO so the per-frame upload can be skipped
      // when its contents are unchanged. Callers must pass distinct keys.
      int cacheKey = 0);

  // Render links straight from the held GraphModel.links snapshot (no per-call
  // Python->C++ marshalling). Filters out dangling links and partitions on
  // relationshipOnly so the two color/draw groups stay separate; whole-prim
  // relationship ends are inset by the shader via the isPrimTarget attribute.
  void renderLinksFromGraph(
      const GraphModel& graph,
      const float* projection4x4,
      const RenderConfig& config,
      float zoom,
      float panX,
      float panY,
      float viewportWidth,
      float viewportHeight,
      float dimming,
      bool drawSelected,
      bool relationshipOnly,
      const float* baseColor = nullptr,
      const float* selectedColor = nullptr,
      const float* hoveredColor = nullptr,
      const float* highlightedColor = nullptr,
      int cacheKey = 0);

  int getCacheHits() const {
    return instanceCache_.hits;
  }
  int getCacheMisses() const {
    return instanceCache_.misses;
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
  int lodForLink(const LinkData& link, float zoom, float linkSampleRate) const;

  // Shared instance upload + draw for both renderLinks (per-call vector) and
  // renderLinksFromGraph (held vector). instancesByLod is keyed by LOD sample
  // count; internalKey = cacheKey*2 + drawSelected selects the persistent VBOs.
  void drawLinkInstances(
      int internalKey,
      std::unordered_map<int, std::vector<float>>& instancesByLod,
      const float* projection4x4,
      const RenderConfig& config,
      float zoom,
      float panX,
      float panY,
      float viewportWidth,
      float viewportHeight,
      float dimming,
      const float* baseColor,
      const float* selectedColor,
      const float* hoveredColor,
      const float* highlightedColor);

  // One persistent instance VBO per (call-site, LOD), plus a CPU copy of its
  // last upload so renderLinks can skip glBufferData when the packed instance
  // data is byte-identical to the previous frame (the dirty gate).
  struct InstanceBucket {
    GLuint vbo = 0;
    std::vector<float> data; // CPU copy of the last upload, for the dirty compare
  };

  std::unordered_map<int, GLuint> refCurveVbos_;
  // [internalKey = cacheKey*2 + drawSelected][numSamples] -> bucket
  std::unordered_map<int, std::unordered_map<int, InstanceBucket>> instanceBuckets_;
  GLuint vao_ = 0;
  ShaderLibrary* shaders_ = nullptr;
  bool initialized_ = false;
  LinkInstanceCache instanceCache_;
};
NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_LINK_RENDER_MANAGER_H
