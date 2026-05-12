// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#define _USE_MATH_DEFINES
#include "render/LinkRenderManager.h"

#include <cmath>
#include <vector>

namespace noodles {

namespace {

bool usesVerticalTargetEndTangent(const LinkData& link) {
  return !link.isDangling && !link.targetNodeId.empty() && link.targetPort.empty();
}

} // namespace

LinkRenderManager::LinkRenderManager() = default;

LinkRenderManager::~LinkRenderManager() {
  cleanup();
}

void LinkRenderManager::initialize(ShaderLibrary* shaders) {
  shaders_ = shaders;
  glGenVertexArrays(1, &vao_);
  initialized_ = true;
}

GLuint LinkRenderManager::ensureReferenceCurveVbo(int numSamples) {
  auto it = refCurveVbos_.find(numSamples);
  if (it != refCurveVbos_.end()) {
    return it->second;
  }

  std::vector<float> vertices;
  vertices.reserve(numSamples * 2 * 7);
  float step = 1.0f / (numSamples - 1);

  auto curveFunc = [](float t) -> std::pair<float, float> {
    float x = t;
    float tClamped = std::max(0.0f, std::min(t, 1.0f));
    float cosT = std::cos(tClamped * static_cast<float>(M_PI) / 2.0f);
    float y = 1.0f - cosT * cosT;
    return {x, y};
  };

  for (int i = 0; i < numSamples; ++i) {
    float t = i * step;
    float prevT = t - step;
    float nextT = t + step;

    auto [px, py] = curveFunc(t);
    auto [ppx, ppy] = curveFunc(prevT);
    auto [npx, npy] = curveFunc(nextT);

    // dir = -1
    vertices.insert(vertices.end(), {px, py, ppx, ppy, npx, npy, -1.0f});
    // dir = +1
    vertices.insert(vertices.end(), {px, py, npx, npy, ppx, ppy, 1.0f});
  }

  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  refCurveVbos_[numSamples] = vbo;
  return vbo;
}

int LinkRenderManager::getLodLevel(int numSegments) const {
  for (const auto& t : LOD_THRESHOLDS) {
    if (numSegments > t.segmentThreshold) {
      return t.sampleCount;
    }
  }
  return 5;
}

void LinkRenderManager::renderLinks(
    const std::vector<LinkData>& links,
    const float* projection4x4,
    float thickness,
    float zoom,
    float panX,
    float panY,
    float viewportWidth,
    float viewportHeight,
    float dimming,
    bool drawSelected,
    const float* baseColor,
    const float* selectedColor,
    const float* hoveredColor) {
  if (!initialized_ || !shaders_ || links.empty()) {
    return;
  }

  auto* shader = shaders_->get("link_poly");
  if (!shader) {
    return;
  }

  // Ensure reference curve VBOs exist
  for (int level : LOD_LEVELS) {
    ensureReferenceCurveVbo(level);
  }

  // Bucket links by LOD level
  float linkSampleRate = 6.0f;
  std::unordered_map<int, std::vector<float>> instancesByLod;

  for (const auto& link : links) {
    if (drawSelected != link.selected) {
      continue;
    }

    float diffX = linkSampleRate / zoom;
    float manhattanLength =
        std::abs(link.end[1] - link.start[1]) + std::abs(link.end[0] - link.start[0]);
    int numSegments = diffX > 0 ? static_cast<int>(manhattanLength / diffX) : 160;
    int lodLevel = getLodLevel(numSegments);

    // Instance data: startPoint(2), endPoint(2), selected(1), hovered(1),
    // verticalEndTangent(1)
    auto& instances = instancesByLod[lodLevel];
    instances.push_back(static_cast<float>(link.start[0]));
    instances.push_back(static_cast<float>(link.start[1]));
    instances.push_back(static_cast<float>(link.end[0]));
    instances.push_back(static_cast<float>(link.end[1]));
    instances.push_back(link.selected ? 1.0f : 0.0f);
    instances.push_back(link.hovered ? 1.0f : 0.0f);
    instances.push_back(usesVerticalTargetEndTangent(link) ? 1.0f : 0.0f);
  }

  // Set up GL state
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint thicknessLoc = shader->getUniformLocation("uThickness");
  if (thicknessLoc >= 0) {
    glUniform1f(thicknessLoc, thickness);
  }

  GLint zoomLoc = shader->getUniformLocation("uZoom");
  if (zoomLoc >= 0) {
    glUniform1f(zoomLoc, zoom);
  }

  float vpWidthWorld = viewportWidth / zoom;
  float vpHeightWorld = viewportHeight / zoom;
  float vpCenterX = panX + vpWidthWorld * 0.5f;
  float vpCenterY = panY + vpHeightWorld * 0.5f;
  float maxVpDim = std::max(vpWidthWorld, vpHeightWorld);

  float lineWidth = thickness * zoom;
  float alphaBase = lineWidth < 1.0f ? std::sqrt(lineWidth) : 1.0f;

  GLint vpCenterLoc = shader->getUniformLocation("uViewportCenter");
  if (vpCenterLoc >= 0) {
    glUniform2f(vpCenterLoc, vpCenterX, vpCenterY);
  }

  GLint maxVpDimLoc = shader->getUniformLocation("uMaxViewportDim");
  if (maxVpDimLoc >= 0) {
    glUniform1f(maxVpDimLoc, maxVpDim);
  }

  GLint dimmingStartLoc = shader->getUniformLocation("uDimmingStart");
  if (dimmingStartLoc >= 0) {
    glUniform1f(dimmingStartLoc, 0.7f);
  }

  GLint dimmingEndLoc = shader->getUniformLocation("uDimmingEnd");
  if (dimmingEndLoc >= 0) {
    glUniform1f(dimmingEndLoc, 0.5f);
  }

  GLint cutoffAlphaLoc = shader->getUniformLocation("uCutoffAlpha");
  if (cutoffAlphaLoc >= 0) {
    glUniform1f(cutoffAlphaLoc, 0.93f);
  }

  GLint dimmingLoc = shader->getUniformLocation("uDimming");
  if (dimmingLoc >= 0) {
    glUniform1f(dimmingLoc, dimming);
  }

  GLint alphaBaseLoc = shader->getUniformLocation("uAlphaBase");
  if (alphaBaseLoc >= 0) {
    glUniform1f(alphaBaseLoc, alphaBase);
  }

  GLint linkColorLoc = shader->getUniformLocation("uLinkColor");
  if (linkColorLoc >= 0) {
    if (baseColor) {
      glUniform3f(linkColorLoc, baseColor[0], baseColor[1], baseColor[2]);
    } else {
      glUniform3f(linkColorLoc, 0.45f, 0.46f, 0.47f); // default gray
    }
  }

  GLint selectedLinkColorLoc = shader->getUniformLocation("uSelectedLinkColor");
  if (selectedLinkColorLoc >= 0) {
    if (selectedColor) {
      glUniform3f(selectedLinkColorLoc, selectedColor[0], selectedColor[1], selectedColor[2]);
    } else {
      glUniform3f(selectedLinkColorLoc, 1.0f, 1.0f, 0.3f); // default yellow
    }
  }

  GLint hoveredLinkColorLoc = shader->getUniformLocation("uHoveredLinkColor");
  if (hoveredLinkColorLoc >= 0) {
    if (hoveredColor) {
      glUniform3f(hoveredLinkColorLoc, hoveredColor[0], hoveredColor[1], hoveredColor[2]);
    } else {
      glUniform3f(hoveredLinkColorLoc, 1.0f, 1.0f, 0.0f); // default yellow
    }
  }

  glBindVertexArray(vao_);

  constexpr int instanceStride = 7 * 4; // 7 floats
  constexpr int refStride = 7 * 4; // 7 floats

  for (auto& [numSamples, instanceData] : instancesByLod) {
    if (instanceData.empty()) {
      continue;
    }

    GLuint refVbo = refCurveVbos_[numSamples];
    if (refVbo == 0) {
      continue;
    }

    // Upload instance data
    if (instanceVbos_.find(numSamples) == instanceVbos_.end()) {
      GLuint vbo = 0;
      glGenBuffers(1, &vbo);
      instanceVbos_[numSamples] = vbo;
    }

    GLuint instanceVbo = instanceVbos_[numSamples];
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(
        GL_ARRAY_BUFFER, instanceData.size() * sizeof(float), instanceData.data(), GL_DYNAMIC_DRAW);

    // Instance attribs (divisor = 1)
    // (4) startPoint vec2 at offset 0
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, instanceStride, nullptr);
    glVertexAttribDivisor(4, 1);

    // (5) endPoint vec2 at offset 8
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, instanceStride, (void*)8);
    glVertexAttribDivisor(5, 1);

    // (6) selected float at offset 16
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, instanceStride, (void*)16);
    glVertexAttribDivisor(6, 1);

    // (7) hovered float at offset 20
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, instanceStride, (void*)20);
    glVertexAttribDivisor(7, 1);

    // (8) verticalEndTangent float at offset 24
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, instanceStride, (void*)24);
    glVertexAttribDivisor(8, 1);

    // Reference curve attribs (divisor = 0)
    glBindBuffer(GL_ARRAY_BUFFER, refVbo);

    // (0) refPosition vec2 at offset 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, refStride, nullptr);
    glVertexAttribDivisor(0, 0);

    // (1) prevPosition vec2 at offset 8
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, refStride, (void*)8);
    glVertexAttribDivisor(1, 0);

    // (2) nextPosition vec2 at offset 16
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, refStride, (void*)16);
    glVertexAttribDivisor(2, 0);

    // (3) dir float at offset 24
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, refStride, (void*)24);
    glVertexAttribDivisor(3, 0);

    int instanceCount = static_cast<int>(instanceData.size()) / 7;
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, numSamples * 2, instanceCount);
  }

  // Reset attrib divisors
  for (int i = 0; i < 9; ++i) {
    glDisableVertexAttribArray(i);
    glVertexAttribDivisor(i, 0);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  shader->release();

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}

void LinkRenderManager::invalidateCache() {
  instanceCache_.invalidate();
}

void LinkRenderManager::cleanup() {
  for (auto& [level, vbo] : refCurveVbos_) {
    if (vbo) {
      glDeleteBuffers(1, &vbo);
    }
  }
  refCurveVbos_.clear();

  for (auto& [level, vbo] : instanceVbos_) {
    if (vbo) {
      glDeleteBuffers(1, &vbo);
    }
  }
  instanceVbos_.clear();

  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }

  shaders_ = nullptr;
  initialized_ = false;
  instanceCache_.clear();
}

} // namespace noodles
