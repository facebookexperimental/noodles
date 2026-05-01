// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_GRAPH_RENDERER_H
#define NOODLES_RENDER_GRAPH_RENDERER_H

#include "core/NodeData.h"
#include "core/RenderConfig.h"
#include "core/api.h"
#include "render/FontAtlas.h"
#include "render/LinkRenderManager.h"
#include "render/NodeRenderManager.h"
#include "render/ShaderLibrary.h"
#include "render/StickerRenderer.h"
#include "render/TextRenderManager.h"

#include <array>
#include <string>
#include <vector>

namespace noodles {

struct NOODLES_API RenderParams {
  float zoom = 1.0f;
  float panX = 0.0f;
  float panY = 0.0f;
  int viewportWidth = 0;
  int viewportHeight = 0;
  float cornerRadius = 8.0f;
  std::array<float, 4> innerStrokeColor = {1.0f, 1.0f, 1.0f, 0.5f};
  float linkThickness = 10.0f;
  float linkDimming = 1.0f;
  bool drawNodes = true;
  bool drawLinks = true;
  bool drawText = true;
  bool drawStickers = true;
  bool textChanged = false;

  // Pre-generated vertex data provided by the caller
  const std::vector<NodeVertex>* nodeVertices = nullptr;
  uint32_t nodeStrokeColor = 0;
  uint32_t nodeGeneration = 0;
  bool nodeSelectionChanged = false;
  const std::vector<NodeVertex>* stickerVertices = nullptr;

  RenderConfig renderConfig;
};

class NOODLES_API GraphRenderer {
 public:
  GraphRenderer();
  ~GraphRenderer();

  GraphRenderer(const GraphRenderer&) = delete;
  GraphRenderer& operator=(const GraphRenderer&) = delete;
  GraphRenderer(GraphRenderer&&) = delete;
  GraphRenderer& operator=(GraphRenderer&&) = delete;

  void initialize(
      const std::string& assetsPath,
      const std::string& fontAtlasPath,
      const std::string& fontMetricsPath);

  void render(const GraphModel& graph, const RenderParams& params);

  void invalidateNode(const std::string& nodeId);
  void invalidateAll();
  void cleanup();

  ShaderLibrary& shaderLibrary() {
    return shaders_;
  }
  FontAtlas& fontAtlas() {
    return fontAtlas_;
  }
  NodeRenderManager& nodeManager() {
    return nodeManager_;
  }
  LinkRenderManager& linkManager() {
    return linkManager_;
  }
  TextRenderManager& textManager() {
    return textManager_;
  }
  StickerRenderer& stickerRenderer() {
    return stickerRenderer_;
  }

 private:
  void computeProjection(const RenderParams& params, float* out4x4) const;

  ShaderLibrary shaders_;
  FontAtlas fontAtlas_;
  NodeRenderManager nodeManager_;
  LinkRenderManager linkManager_;
  TextRenderManager textManager_;
  StickerRenderer stickerRenderer_;
  bool initialized_ = false;
};

} // namespace noodles

#endif // NOODLES_RENDER_GRAPH_RENDERER_H
