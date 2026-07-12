// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/NodeTransformFrame.h"

#include <algorithm>

namespace noodles {

NodeTransformFrame::NodeTransformFrame(int initialCapacityTexels)
    : capacity_(std::max(1, initialCapacityTexels)) {}

NodeTransformFrame::~NodeTransformFrame() {
  cleanup();
}

int NodeTransformFrame::allocateSlot() {
  if (!freeSlots_.empty()) {
    int slot = freeSlots_.back();
    freeSlots_.pop_back();
    return slot;
  }
  return nextSlot_++;
}

void NodeTransformFrame::growCapacity(int neededTexels) {
  while (capacity_ < neededTexels && capacity_ < kMaxCapacity) {
    capacity_ = std::min(capacity_ * 2, kMaxCapacity);
  }
}

void NodeTransformFrame::syncFromNodes(const std::unordered_map<std::string, NodeData>& nodes) {
  // Prune deleted nodes and reclaim their slots.
  for (auto it = idToIndex_.begin(); it != idToIndex_.end();) {
    if (nodes.find(it->first) == nodes.end()) {
      freeSlots_.push_back(it->second);
      basePositions_.erase(it->first);
      transforms_.erase(it->first);
      it = idToIndex_.erase(it);
      dirty_ = true;
    } else {
      ++it;
    }
  }

  // Assign slots + base/zero-offset to new nodes; existing nodes keep theirs.
  for (const auto& [nodeId, node] : nodes) {
    if (idToIndex_.find(nodeId) != idToIndex_.end()) {
      continue;
    }
    idToIndex_[nodeId] = allocateSlot();
    basePositions_[nodeId] = {node.position[0], node.position[1]};
    transforms_[nodeId] = {0.0f, 0.0f};
    dirty_ = true;
  }

  // CPU-side only; the GL buffer resizes lazily on upload().
  growCapacity(nextSlot_);
}

float NodeTransformFrame::shaderIndex(const std::string& nodeId) const {
  auto it = idToIndex_.find(nodeId);
  if (it == idToIndex_.end() || it->second >= capacity_) {
    return -1.0f;
  }
  return static_cast<float>(it->second);
}

Vec2d NodeTransformFrame::basePosition(const std::string& nodeId) const {
  auto it = basePositions_.find(nodeId);
  if (it == basePositions_.end()) {
    return Vec2d(0.0, 0.0);
  }
  return Vec2d(it->second.first, it->second.second);
}

Vec2d NodeTransformFrame::offset(const std::string& nodeId) const {
  auto it = transforms_.find(nodeId);
  if (it == transforms_.end()) {
    return Vec2d(0.0, 0.0);
  }
  return Vec2d(it->second.first, it->second.second);
}

Vec2d NodeTransformFrame::liveOrigin(const std::string& nodeId, const Vec2d& fallback) const {
  // No transform slot (e.g. a node created before the next sync): geometry draws
  // unshifted at the fallback (the GraphModel snapshot position).
  if (shaderIndex(nodeId) < 0.0f) {
    return fallback;
  }
  // Geometry bakes at the base frame and rides the live move offset on the GPU,
  // so this is where it actually draws.
  const Vec2d base = basePosition(nodeId);
  const Vec2d off = offset(nodeId);
  return Vec2d(base[0] + off[0], base[1] + off[1]);
}

void NodeTransformFrame::translate(const std::string& nodeId, double dx, double dy) {
  auto it = transforms_.find(nodeId);
  if (it == transforms_.end()) {
    return;
  }
  it->second.first += static_cast<float>(dx);
  it->second.second += static_cast<float>(dy);
  dirty_ = true;
}

void NodeTransformFrame::reset() {
  idToIndex_.clear();
  transforms_.clear();
  basePositions_.clear();
  freeSlots_.clear();
  nextSlot_ = 0;
  dirty_ = true;
}

void NodeTransformFrame::ensureBuffer() {
  if (buffer_ && glBufferCapacity_ >= capacity_) {
    return;
  }
  if (!buffer_) {
    glGenBuffers(1, &buffer_);
  }
  if (!texture_) {
    glGenTextures(1, &texture_);
  }
  glBindBuffer(GL_TEXTURE_BUFFER, buffer_);
  glBufferData(
      GL_TEXTURE_BUFFER,
      static_cast<GLsizeiptr>(capacity_) * 2 * sizeof(float),
      nullptr,
      GL_DYNAMIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  glBindTexture(GL_TEXTURE_BUFFER, texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, buffer_);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  glBufferCapacity_ = capacity_;
  // The (re)allocated store is undefined until the offsets are uploaded.
  dirty_ = true;
}

void NodeTransformFrame::upload() {
  ensureBuffer();
  if (!buffer_ || !dirty_) {
    return;
  }

  // Reuse the staging buffer (zeroed in place) — a drag uploads every frame.
  uploadStaging_.assign(static_cast<size_t>(capacity_) * 2, 0.0f);
  for (const auto& [nodeId, slot] : idToIndex_) {
    auto tIt = transforms_.find(nodeId);
    if (tIt != transforms_.end() && slot < capacity_) {
      uploadStaging_[slot * 2] = tIt->second.first;
      uploadStaging_[slot * 2 + 1] = tIt->second.second;
    }
  }

  glBindBuffer(GL_TEXTURE_BUFFER, buffer_);
  glBufferSubData(
      GL_TEXTURE_BUFFER,
      0,
      static_cast<GLsizeiptr>(capacity_) * 2 * sizeof(float),
      uploadStaging_.data());
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  dirty_ = false;
}

void NodeTransformFrame::bindToShader(GLSLProgram* shader, int textureUnit) {
  if (!shader || !texture_) {
    return;
  }
  GLint samplerLoc = shader->getUniformLocation("uNodeTransforms");
  if (samplerLoc >= 0) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_BUFFER, texture_);
    glUniform1i(samplerLoc, textureUnit);
    glActiveTexture(GL_TEXTURE0);
  }
}

void NodeTransformFrame::cleanup() {
  if (texture_) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  if (buffer_) {
    glDeleteBuffers(1, &buffer_);
    buffer_ = 0;
  }
  glBufferCapacity_ = 0;
  idToIndex_.clear();
  transforms_.clear();
  basePositions_.clear();
  freeSlots_.clear();
  nextSlot_ = 0;
  dirty_ = true;
}

} // namespace noodles
