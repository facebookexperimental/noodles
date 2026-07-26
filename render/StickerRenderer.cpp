// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/StickerRenderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace noodles {

namespace {

std::uint8_t clampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

// Emit two triangles (6 vertices) covering the backdrop rect at the given depth.
void appendStickerQuad(std::vector<NodeVertex>& out, const StickerRenderInput& s, float depth) {
  const float w = s.width;
  const float h = s.height;
  const float x0 = s.x;
  const float y0 = s.y;
  const float x1 = x0 + w;
  const float y1 = y0 + h;

  const std::uint8_t r = clampByte(s.r);
  const std::uint8_t g = clampByte(s.g);
  const std::uint8_t b = clampByte(s.b);
  const std::uint8_t a = clampByte(s.a);
  const float stroke = s.innerStroke;

  // Selected backdrops blend their fill toward the highlight color; the frag
  // shader mixes fragColor toward (sr,sg,sb) by `selected`.
  std::uint8_t sr = 0;
  std::uint8_t sg = 0;
  std::uint8_t sb = 0;
  std::uint8_t sa = 255;
  float selected = 0.0f;
  if (s.selected) {
    sr = kStickerSelectionR;
    sg = kStickerSelectionG;
    sb = kStickerSelectionB;
    sa = kStickerSelectionA;
    selected = kStickerSelectionBlend;
  }

  const auto push = [&](float px, float py, float u, float v) {
    out.emplace_back(px, py, depth, u, v, w, h, r, g, b, a, stroke, sr, sg, sb, sa, selected);
  };

  push(x0, y0, 0.0f, 0.0f);
  push(x1, y0, w, 0.0f);
  push(x0, y1, 0.0f, h);
  push(x1, y0, w, 0.0f);
  push(x1, y1, w, h);
  push(x0, y1, 0.0f, h);
}

} // namespace

StickerRenderer::StickerRenderer() = default;

StickerRenderer::~StickerRenderer() {
  cleanup();
}

void StickerRenderer::initialize(ShaderLibrary* shaders) {
  shaders_ = shaders;
  initialized_ = true;
}

std::vector<NodeVertex> StickerRenderer::buildStickerVertices(
    const std::vector<StickerRenderInput>& stickers) {
  std::vector<NodeVertex> vertices;
  const size_t count = stickers.size();
  if (count == 0) {
    return vertices;
  }
  vertices.reserve(count * 6);

  // Order back-to-front by priority; ties keep original list order (stable) so
  // the arrangement is deterministic. Lower priority is drawn first (further
  // back / more negative depth); higher priority approaches kStickerDepthFront.
  std::vector<size_t> order(count);
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(), [&stickers](size_t lhs, size_t rhs) {
    return stickers[lhs].priority < stickers[rhs].priority;
  });

  for (size_t renderIndex = 0; renderIndex < count; ++renderIndex) {
    const StickerRenderInput& sticker = stickers[order[renderIndex]];
    const float depth =
        kStickerDepthFront - static_cast<float>(count - 1 - renderIndex) * kStickerDepthStep;
    appendStickerQuad(vertices, sticker, depth);
  }

  return vertices;
}

void StickerRenderer::renderStickerData(
    const std::vector<StickerRenderInput>& stickers,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& strokeColor) {
  renderStickers(buildStickerVertices(stickers), projection4x4, cornerRadius, strokeColor);
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
