// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/NodeRenderManager.h"

#include <array>

namespace noodles {

NodeRenderManager::NodeRenderManager() = default;

NodeRenderManager::~NodeRenderManager() {
  cleanup();
}

void NodeRenderManager::initialize(ShaderLibrary* shaders) {
  shaders_ = shaders;
  initialized_ = true;
}

void NodeRenderManager::renderNodes(
    const std::vector<NodeVertex>& vertices,
    uint32_t strokeColor,
    uint32_t generation,
    bool selectionChanged,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& innerStrokeColor) {
  if (!initialized_ || !shaders_ || vertices.empty()) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint radiusLoc = shader->getUniformLocation("uCornerRadius");
  if (radiusLoc >= 0) {
    glUniform1f(radiusLoc, cornerRadius);
  }

  GLint strokeLoc = shader->getUniformLocation("uInnerStrokeColor");
  if (strokeLoc >= 0) {
    glUniform4fv(strokeLoc, 1, innerStrokeColor.data());
  }

  auto& batch = batches_[strokeColor];
  createOrUpdateVBO(batch, vertices, generation, selectionChanged);

  glBindVertexArray(batch.vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);

  shader->release();
}

std::vector<NodeVertex> NodeRenderManager::buildPortQuadVertices(
    float portX,
    float portY,
    float portW,
    float portH,
    float depth,
    const std::array<float, 4>& color) {
  auto r = static_cast<uint8_t>(color[0] * 255.0f);
  auto g = static_cast<uint8_t>(color[1] * 255.0f);
  auto b = static_cast<uint8_t>(color[2] * 255.0f);
  auto a = static_cast<uint8_t>(color[3] * 255.0f);

  // Use selected = -1.0f sentinel to trigger per-vertex stroke color in the shader
  std::vector<NodeVertex> vertices;
  vertices.reserve(6);

  // Triangle 1: top-left, top-right, bottom-right
  vertices.emplace_back(
      portX, portY, depth, 0.0f, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW, portY, depth, portW, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW,
      portY + portH,
      depth,
      portW,
      portH,
      portW,
      portH,
      r,
      g,
      b,
      a,
      0.0f,
      r,
      g,
      b,
      a,
      -1.0f);

  // Triangle 2: top-left, bottom-right, bottom-left
  vertices.emplace_back(
      portX, portY, depth, 0.0f, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW,
      portY + portH,
      depth,
      portW,
      portH,
      portW,
      portH,
      r,
      g,
      b,
      a,
      0.0f,
      r,
      g,
      b,
      a,
      -1.0f);
  vertices.emplace_back(
      portX, portY + portH, depth, 0.0f, portH, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);

  return vertices;
}

void NodeRenderManager::drawVerticesImmediate(
    const std::vector<NodeVertex>& vertices,
    GLSLProgram* shader,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& strokeColor) {
  auto packedData = NodeVertex::packBatch(vertices);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint radiusLoc = shader->getUniformLocation("uCornerRadius");
  if (radiusLoc >= 0) {
    glUniform1f(radiusLoc, cornerRadius);
  }

  GLint strokeLoc = shader->getUniformLocation("uInnerStrokeColor");
  if (strokeLoc >= 0) {
    glUniform4fv(strokeLoc, 1, strokeColor.data());
  }

  GLuint vao = 0;
  GLuint vbo = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(packedData.size()),
      packedData.data(),
      GL_STREAM_DRAW);

  NodeVertex::setupVertexAttribs();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);

  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &vao);

  shader->release();
}

void NodeRenderManager::renderPortHighlight(
    float portX,
    float portY,
    float portW,
    float portH,
    float depth,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& highlightColor) {
  if (!initialized_ || !shaders_) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices = buildPortQuadVertices(portX, portY, portW, portH, depth, highlightColor);
  drawVerticesImmediate(vertices, shader, projection4x4, cornerRadius, highlightColor);
}

void NodeRenderManager::createOrUpdateVBO(
    RenderBatch& batch,
    const std::vector<NodeVertex>& vertices,
    uint32_t generation,
    bool selectionChanged) {
  int vertexCount = static_cast<int>(vertices.size());
  auto packedData = NodeVertex::packBatch(vertices);
  auto dataSize = static_cast<GLsizeiptr>(packedData.size());

  if (batch.vbo == 0) {
    setupVAO(batch, dataSize, packedData.data());
    batch.cachedGeneration = generation;
    batch.cachedVertexCount = vertexCount;
    return;
  }

  bool unchanged = batch.cachedGeneration == generation && batch.cachedVertexCount == vertexCount &&
      !selectionChanged;
  if (unchanged) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
  if (batch.cachedVertexCount == vertexCount) {
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, packedData.data());
  } else {
    glBufferData(GL_ARRAY_BUFFER, dataSize, packedData.data(), GL_DYNAMIC_DRAW);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  batch.cachedGeneration = generation;
  batch.cachedVertexCount = vertexCount;
}

void NodeRenderManager::setupVAO(RenderBatch& batch, GLsizeiptr dataSize, const void* data) {
  glGenBuffers(1, &batch.vbo);
  glGenVertexArrays(1, &batch.vao);

  glBindVertexArray(batch.vao);
  glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
  glBufferData(GL_ARRAY_BUFFER, dataSize, data, GL_DYNAMIC_DRAW);

  NodeVertex::setupVertexAttribs();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void NodeRenderManager::invalidateAll() {
  vertexCache_.invalidateAll();
}

void NodeRenderManager::invalidateNode(const std::string& nodeId) {
  vertexCache_.invalidateNode(nodeId);
}

void NodeRenderManager::clearCache() {
  vertexCache_.clear();
  for (auto& [color, batch] : batches_) {
    if (batch.vbo) {
      glDeleteBuffers(1, &batch.vbo);
    }
    if (batch.vao) {
      glDeleteVertexArrays(1, &batch.vao);
    }
  }
  batches_.clear();
}

void NodeRenderManager::cleanup() {
  clearCache();
  shaders_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
