// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_TEXT_RENDER_MANAGER_H
#define NOODLES_RENDER_TEXT_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/RenderConfig.h"
#include "core/api.h"
#include "render/FontAtlas.h"
#include "render/NodeTransformFrame.h"
#include "render/ShaderLibrary.h"
#include "render/TextVertexCache.h"

#include <GL/glew.h>

#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace noodles {

class GraphNodeRenderer;

/// Input to assembleNodeTextBuffer. depth/nodeIndex are re-baked over the
/// cached vertices so a z-order or slot change doesn't require re-layout.
struct NodeTextAssemblyItem {
  const std::vector<float>* vertices = nullptr;
  float depth = 0.0f;
  float nodeIndex = 0.0f;
};

class NOODLES_API TextRenderManager {
 public:
  TextRenderManager();
  ~TextRenderManager();

  TextRenderManager(const TextRenderManager&) = delete;
  TextRenderManager& operator=(const TextRenderManager&) = delete;
  TextRenderManager(TextRenderManager&&) = delete;
  TextRenderManager& operator=(TextRenderManager&&) = delete;

  void initialize(ShaderLibrary* shaders, FontAtlas* fontAtlas);

  // The shared node-local coordinate frame (base positions + move offsets +
  // transform texture). Non-owning; the same frame is shared with
  // NodeRenderManager so text and node quads move together.
  void setTransformFrame(NodeTransformFrame* frame) {
    transformFrame_ = frame;
  }

  double calculateTextWidth(const std::string& text, double fontSize) const;

  std::pair<double, int> generateTextVertices(
      const std::string& text,
      double cursorX,
      double cursorY,
      float depth,
      double scale,
      std::vector<float>& vertexData,
      float nodeIndex = 0.0f) const;

  void renderText(
      const std::vector<float>& vertexData,
      const float* projection4x4,
      const std::array<float, 4>& color);

  void renderNodeText(
      const std::vector<float>& vertexData,
      const float* projection4x4,
      const std::array<float, 4>& color,
      bool needsFullUpdate);

  // Full node text orchestration
  std::vector<float> generateNodeTextVertices(
      std::unordered_map<std::string, NodeData>& nodes,
      const RenderConfig& config);

  bool renderNodeTextFull(
      std::unordered_map<std::string, NodeData>& nodes,
      const float* projection4x4,
      float zoom,
      bool textChanged,
      const RenderConfig& config,
      double panX = 0.0,
      double panY = 0.0,
      double viewportWidth = 0.0,
      double viewportHeight = 0.0,
      bool cullOffscreen = false);

  // Off-screen text culling: from the per-node text slices and node AABBs, build
  // the (firstVertex, vertexCount) draw ranges for nodes whose box intersects the
  // world-space viewport rect [vpMin, vpMax]. The big text buffer stays intact
  // (no rebuild on pan/zoom) — only the visible ranges are drawn, via
  // glMultiDrawArrays. Pure / no GL, so it is unit-tested directly.
  //
  // transformFrame (optional) supplies each node's base + live move offset, so
  // the cull tests the SAME world position the GPU draws the glyphs at: glyphs
  // bake at the base and ride the move offset via the transform texture. Pass
  // nullptr to cull on the raw GraphModel snapshot position (pure-math tests) —
  // but the snapshot is NOT re-synced on a drag, so culling on it strands a
  // dragged node's on-screen text; the render path must pass the frame.
  static void computeVisibleTextRanges(
      const std::unordered_map<std::string, NodeData>& nodes,
      const std::unordered_map<std::string, std::pair<int, int>>& slices,
      double vpMinX,
      double vpMinY,
      double vpMaxX,
      double vpMaxY,
      std::vector<GLint>& outFirsts,
      std::vector<GLsizei>& outCounts,
      const NodeTransformFrame* transformFrame = nullptr);

  // True when the next render will re-lay-out (a force-full is pending). The
  // Python caller re-syncs GraphModel.nodes whenever this is set, so a rebuild
  // never reads stale node data even on a frame that didn't set textChanged.
  bool needsTextRebuild() const {
    return nodeTextNeedsRebuild_;
  }

  void markNodeTextDirty(bool needsRebuild = false);

  // Drop cached text layout (per-node blobs + slices) and force a full text
  // rebuild on the next render (called when the graph is (re)loaded). The shared
  // node positions live in NodeTransformFrame and are reset there.
  void resetPositionCaches();

  // Patch a single node's text depth in the held buffer (selection
  // bring-to-front) without a full re-layout. Returns true if the node's slice
  // was found.
  bool patchNodeTextDepth(const std::string& nodeId, int zOrder);

  void drawTextVertices(
      const std::vector<float>& vertexData,
      const float* projection4x4,
      const std::array<float, 4>& color,
      bool disableDepth = false);

  const std::vector<float>& getNodeTextVertexData() const {
    return nodeTextVertexData_;
  }

  // Test-only.
  const TextVertexCache& textVertexCacheForTest() const {
    return textVertexCache_;
  }

  // Pure (no GL/FontAtlas) for unit-testability.
  static std::vector<float> assembleNodeTextBuffer(
      const std::vector<NodeTextAssemblyItem>& items,
      std::vector<int>& outStarts,
      std::vector<int>& outCharCounts);

  void cleanup();

 private:
  // Lay out one node's text into `out`. Requires fontAtlas_.
  void layoutSingleNode(
      const std::string& nodeId,
      const NodeData& node,
      float shaderNodeIndex,
      float depth,
      const RenderConfig& config,
      std::vector<float>& out) const;
  void setupTextShader(const float* projection4x4, const std::array<float, 4>& color);

  ShaderLibrary* shaders_ = nullptr;
  FontAtlas* fontAtlas_ = nullptr;
  bool initialized_ = false;
  NodeTransformFrame* transformFrame_ = nullptr; // shared, non-owning

  GLuint textVbo_ = 0;
  GLuint textVao_ = 0;

  GLuint nodeTextVbo_ = 0;
  GLuint nodeTextVao_ = 0;
  int nodeTextVertexCount_ = 0;

  // Node text orchestration state
  std::vector<float> nodeTextVertexData_;
  bool nodeTextDirty_ = false;
  bool nodeTextNeedsRebuild_ = true;
  // nodeId -> (textStartIndex, textNumChars) into nodeTextVertexData_, used to
  // patch one node's depth in place (selection bring-to-front).
  std::unordered_map<std::string, std::pair<int, int>> nodeTextSlices_;
  // Only nodes whose signature changed are re-laid-out; the rest are reused.
  TextVertexCache textVertexCache_;
};

} // namespace noodles

#endif // NOODLES_RENDER_TEXT_RENDER_MANAGER_H
