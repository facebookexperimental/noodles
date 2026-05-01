// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/StickerRenderer.h"

#include <array>

namespace noodles {

StickerRenderer::StickerRenderer() = default;

StickerRenderer::~StickerRenderer() {
  cleanup();
}

void StickerRenderer::initialize(ShaderLibrary* shaders) {
  shaders_ = shaders;
  initialized_ = true;
}

void StickerRenderer::renderStickers(
    const std::vector<NodeVertex>& vertices,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& strokeColor) {
  if (!initialized_ || !shaders_ || vertices.empty()) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto packedData = NodeVertex::packBatch(vertices);
  auto dataSize = static_cast<GLsizeiptr>(packedData.size());

  if (vbo_ == 0) {
    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, dataSize, packedData.data(), GL_DYNAMIC_DRAW);

    NodeVertex::setupVertexAttribs();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  } else {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, dataSize, packedData.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    glUniform4fv(strokeLoc, 1, strokeColor.data());
  }

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);

  shader->release();
}

void StickerRenderer::markDirty() {
  // No-op in the C++ version since vertices are provided externally
}

void StickerRenderer::cleanup() {
  if (vbo_) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  shaders_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
