// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/GraphRenderer.h"

#include <GL/glew.h>

#include <array>

namespace noodles {

GraphRenderer::GraphRenderer() = default;

GraphRenderer::~GraphRenderer() {
  cleanup();
}

void GraphRenderer::initialize(
    const std::string& assetsPath,
    const std::string& fontAtlasPath,
    const std::string& fontMetricsPath) {
  shaders_.initialize(assetsPath);
  fontAtlas_.initialize(fontAtlasPath, fontMetricsPath);
  nodeManager_.initialize(&shaders_);
  linkManager_.initialize(&shaders_);
  textManager_.initialize(&shaders_, &fontAtlas_);
  stickerRenderer_.initialize(&shaders_);
  initialized_ = true;
}

void GraphRenderer::render(const GraphModel& graph, const RenderParams& params) {
  if (!initialized_) {
    return;
  }

  std::array<float, 16> projection{};
  computeProjection(params, projection.data());

  // Stickers render behind everything
  if (params.drawStickers && params.stickerVertices && !params.stickerVertices->empty()) {
    stickerRenderer_.renderStickers(
        *params.stickerVertices, projection.data(), params.cornerRadius, params.innerStrokeColor);
  }

  // Links: non-selected first, then selected for proper Z-ordering
  if (params.drawLinks) {
    linkManager_.renderLinks(
        graph.links,
        projection.data(),
        params.linkThickness,
        params.zoom,
        params.panX,
        params.panY,
        static_cast<float>(params.viewportWidth),
        static_cast<float>(params.viewportHeight),
        params.linkDimming,
        false);

    linkManager_.renderLinks(
        graph.links,
        projection.data(),
        params.linkThickness,
        params.zoom,
        params.panX,
        params.panY,
        static_cast<float>(params.viewportWidth),
        static_cast<float>(params.viewportHeight),
        params.linkDimming,
        true);
  }

  if (params.drawNodes && params.nodeVertices && !params.nodeVertices->empty()) {
    nodeManager_.renderNodes(
        *params.nodeVertices,
        params.nodeStrokeColor,
        params.nodeGeneration,
        params.nodeSelectionChanged,
        projection.data(),
        params.cornerRadius,
        params.innerStrokeColor);
  }

  if (params.drawText) {
    // Use a mutable copy of nodes for text vertex generation
    auto mutableNodes = graph.nodes;
    textManager_.renderNodeTextFull(
        mutableNodes, projection.data(), params.zoom, params.textChanged, params.renderConfig);
  }
}

void GraphRenderer::invalidateNode(const std::string& nodeId) {
  nodeManager_.invalidateNode(nodeId);
}

void GraphRenderer::invalidateAll() {
  nodeManager_.invalidateAll();
}

void GraphRenderer::cleanup() {
  stickerRenderer_.cleanup();
  textManager_.cleanup();
  linkManager_.cleanup();
  nodeManager_.cleanup();
  fontAtlas_.cleanup();
  shaders_.cleanup();
  initialized_ = false;
}

void GraphRenderer::computeProjection(const RenderParams& params, float* out4x4) const {
  // Orthographic projection: world coords → NDC
  // Maps [panX, panX + w/zoom] × [panY, panY + h/zoom] to [-1,1]
  float w = static_cast<float>(params.viewportWidth) / params.zoom;
  float h = static_cast<float>(params.viewportHeight) / params.zoom;

  float left = params.panX;
  float right = params.panX + w;
  float top = params.panY;
  float bottom = params.panY + h;
  float nearVal = -1000.0f;
  float farVal = 1000.0f;

  // Column-major orthographic matrix
  out4x4[0] = 2.0f / (right - left);
  out4x4[1] = 0.0f;
  out4x4[2] = 0.0f;
  out4x4[3] = 0.0f;

  out4x4[4] = 0.0f;
  out4x4[5] = 2.0f / (top - bottom);
  out4x4[6] = 0.0f;
  out4x4[7] = 0.0f;

  out4x4[8] = 0.0f;
  out4x4[9] = 0.0f;
  out4x4[10] = -2.0f / (farVal - nearVal);
  out4x4[11] = 0.0f;

  out4x4[12] = -(right + left) / (right - left);
  out4x4[13] = -(top + bottom) / (top - bottom);
  out4x4[14] = -(farVal + nearVal) / (farVal - nearVal);
  out4x4[15] = 1.0f;
}

} // namespace noodles
