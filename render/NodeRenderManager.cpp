// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/NodeRenderManager.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace noodles {

namespace {
uint8_t floatColorToByte(float v) {
  return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}
} // namespace

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
  if (batch.vao == 0) {
    // setupVAO bailed out because the GL context was unusable (e.g. mid stage
    // reopen on NVIDIA Windows).  Skip the draw — we'll retry next frame
    // once the driver is healthy.
    shader->release();
    return;
  }

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
  auto r = floatColorToByte(color[0]);
  auto g = floatColorToByte(color[1]);
  auto b = floatColorToByte(color[2]);
  auto a = floatColorToByte(color[3]);

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

std::vector<NodeVertex> NodeRenderManager::buildCircleEndpointVertices(
    float cx,
    float cy,
    float radius,
    float depth,
    const std::array<float, 4>& fillColor,
    const std::array<float, 4>& strokeColor,
    float strokeWidth) {
  strokeWidth = std::max(strokeWidth, 0.0f);

  float diameter = radius * 2.0f;
  float left = cx - radius;
  float top = cy - radius;

  auto fr = floatColorToByte(fillColor[0]);
  auto fg = floatColorToByte(fillColor[1]);
  auto fb = floatColorToByte(fillColor[2]);
  auto fa = floatColorToByte(fillColor[3]);

  auto sr = floatColorToByte(strokeColor[0]);
  auto sg = floatColorToByte(strokeColor[1]);
  auto sb = floatColorToByte(strokeColor[2]);
  auto sa = floatColorToByte(strokeColor[3]);

  // clang-format off
  return {
      {left,            top,            depth, 0.0f,     0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top,            depth, diameter, 0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top + diameter, depth, diameter, diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left,            top,            depth, 0.0f,     0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top + diameter, depth, diameter, diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left,            top + diameter, depth, 0.0f,     diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
  };
  // clang-format on
}

std::vector<NodeVertex> NodeRenderManager::buildTriangleEndpointVertices(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float depth,
    const std::array<float, 4>& color) {
  float minX = std::min({x0, x1, x2});
  float maxX = std::max({x0, x1, x2});
  float minY = std::min({y0, y1, y2});
  float maxY = std::max({y0, y1, y2});
  float w = std::max(maxX - minX, 1.0f);
  float h = std::max(maxY - minY, 1.0f);

  auto r = floatColorToByte(color[0]);
  auto g = floatColorToByte(color[1]);
  auto b = floatColorToByte(color[2]);
  auto a = floatColorToByte(color[3]);

  return {
      {x0, y0, depth, x0 - minX, y0 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
      {x1, y1, depth, x1 - minX, y1 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
      {x2, y2, depth, x2 - minX, y2 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
  };
}

void NodeRenderManager::renderEndpointCircle(
    float cx,
    float cy,
    float radius,
    float depth,
    const float* projection4x4,
    const std::array<float, 4>& fillColor,
    const std::array<float, 4>& strokeColor,
    float strokeWidth) {
  if (!initialized_ || !shaders_ || radius <= 0.0f) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices =
      buildCircleEndpointVertices(cx, cy, radius, depth, fillColor, strokeColor, strokeWidth);
  drawVerticesImmediate(vertices, shader, projection4x4, radius, {0.0f, 0.0f, 0.0f, 0.0f});
}

void NodeRenderManager::renderEndpointTriangle(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float depth,
    const float* projection4x4,
    const std::array<float, 4>& color) {
  if (!initialized_ || !shaders_) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices = buildTriangleEndpointVertices(x0, y0, x1, y1, x2, y2, depth, color);
  drawVerticesImmediate(vertices, shader, projection4x4, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f});
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
  if (batch.vbo == 0 || batch.vao == 0) {
    // GL context unusable.  Bail rather than issue draws against the default
    // object — that's GL_INVALID_OPERATION in core profile and crashes the
    // NVIDIA driver on its worker thread.  renderNodes will retry next frame.
    if (batch.vbo != 0) {
      glDeleteBuffers(1, &batch.vbo);
      batch.vbo = 0;
    }
    if (batch.vao != 0) {
      glDeleteVertexArrays(1, &batch.vao);
      batch.vao = 0;
    }
    return;
  }

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
