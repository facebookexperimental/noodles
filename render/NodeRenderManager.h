// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_NODE_RENDER_MANAGER_H
#define NOODLES_RENDER_NODE_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/api.h"
#include "core/pragmas.h"
#include "render/NodeVertexCache.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API RenderBatch {
  GLuint vao = 0;
  GLuint vbo = 0;
  uint32_t cachedGeneration = 0;
  int cachedVertexCount = 0;
};

class NOODLES_API NodeRenderManager {
 public:
  NodeRenderManager();
  ~NodeRenderManager();

  NodeRenderManager(const NodeRenderManager&) = delete;
  NodeRenderManager& operator=(const NodeRenderManager&) = delete;
  NodeRenderManager(NodeRenderManager&&) = delete;
  NodeRenderManager& operator=(NodeRenderManager&&) = delete;

  void initialize(ShaderLibrary* shaders);

  NodeVertexCache& vertexCache() {
    return vertexCache_;
  }
  const NodeVertexCache& vertexCache() const {
    return vertexCache_;
  }

  void renderNodes(
      const std::vector<NodeVertex>& vertices,
      uint32_t strokeColor,
      uint32_t generation,
      bool selectionChanged,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& innerStrokeColor);

  void renderPortHighlight(
      float portX,
      float portY,
      float portW,
      float portH,
      float depth,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& highlightColor);

  void invalidateAll();
  void invalidateNode(const std::string& nodeId);
  void clearCache();
  void cleanup();

 private:
  static std::vector<NodeVertex> buildPortQuadVertices(
      float portX,
      float portY,
      float portW,
      float portH,
      float depth,
      const std::array<float, 4>& color);

  static void drawVerticesImmediate(
      const std::vector<NodeVertex>& vertices,
      GLSLProgram* shader,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& strokeColor);

  void createOrUpdateVBO(
      RenderBatch& batch,
      const std::vector<NodeVertex>& vertices,
      uint32_t generation,
      bool selectionChanged);
  void setupVAO(RenderBatch& batch, GLsizeiptr dataSize, const void* data);

  NodeVertexCache vertexCache_;
  std::unordered_map<uint32_t, RenderBatch> batches_;
  ShaderLibrary* shaders_ = nullptr;
  bool initialized_ = false;
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_NODE_RENDER_MANAGER_H
