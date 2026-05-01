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
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace noodles {

class GraphNodeRenderer;

class NOODLES_API TextRenderManager {
 public:
  TextRenderManager();
  ~TextRenderManager();

  TextRenderManager(const TextRenderManager&) = delete;
  TextRenderManager& operator=(const TextRenderManager&) = delete;
  TextRenderManager(TextRenderManager&&) = delete;
  TextRenderManager& operator=(TextRenderManager&&) = delete;

  void initialize(ShaderLibrary* shaders, FontAtlas* fontAtlas);

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

  void updateTransforms(
      const std::unordered_map<std::string, int>& nodeIdToIndex,
      const std::unordered_map<std::string, std::pair<float, float>>& transforms);

  // Full node text orchestration
  std::vector<float> generateNodeTextVertices(
      std::unordered_map<std::string, NodeData>& nodes,
      const RenderConfig& config);

  bool renderNodeTextFull(
      std::unordered_map<std::string, NodeData>& nodes,
      const float* projection4x4,
      float zoom,
      bool textChanged,
      const RenderConfig& config);

  void markNodeTextDirty(bool needsRebuild = false);
  void updateNodeTextPosition(const std::string& nodeId, double dx, double dy);

  void drawTextVertices(
      const std::vector<float>& vertexData,
      const float* projection4x4,
      const std::array<float, 4>& color,
      bool disableDepth = false);

  const std::vector<float>& getNodeTextVertexData() const {
    return nodeTextVertexData_;
  }

  const std::unordered_map<std::string, int>& getNodeIdToIndex() const {
    return nodeIdToIndex_;
  }

  const std::unordered_map<std::string, std::pair<float, float>>& getNodeTransforms() const {
    return nodeTransforms_;
  }

  void cleanup();

 private:
  void createTransformTexture();
  void setupTextShader(const float* projection4x4, const std::array<float, 4>& color);
  void updateTransformTexture();

  ShaderLibrary* shaders_ = nullptr;
  FontAtlas* fontAtlas_ = nullptr;
  bool initialized_ = false;

  GLuint textVbo_ = 0;
  GLuint textVao_ = 0;

  GLuint nodeTextVbo_ = 0;
  GLuint nodeTextVao_ = 0;
  int nodeTextVertexCount_ = 0;

  GLuint transformTexture_ = 0;
  int transformTextureSize_ = 2048;

  // Node text orchestration state
  std::vector<float> nodeTextVertexData_;
  bool nodeTextDirty_ = false;
  bool nodeTextNeedsRebuild_ = true;
  std::unordered_map<std::string, int> nodeIdToIndex_;
  std::unordered_map<std::string, std::pair<float, float>> nodeTransforms_;
  std::unordered_map<std::string, std::pair<double, double>> nodeBasePositions_;
};

} // namespace noodles

#endif // NOODLES_RENDER_TEXT_RENDER_MANAGER_H
