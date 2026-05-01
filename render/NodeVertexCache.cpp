// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/NodeVertexCache.h"

namespace noodles {

bool NodeVertexCache::isDirty(const std::string& nodeId, const Vec2d& position, const Vec2d& size)
    const {
  auto it = cache_.find(nodeId);
  if (it == cache_.end()) {
    return true;
  }
  const auto& cached = it->second;
  if (cached.dirty || cached.vertices.empty()) {
    return true;
  }
  if (cached.generation < globalGeneration_) {
    return true;
  }
  if (cached.cachedPosition != position || cached.cachedSize != size) {
    return true;
  }
  return false;
}

void NodeVertexCache::update(
    const std::string& nodeId,
    const std::vector<NodeVertex>& vertices,
    const Vec2d& position,
    const Vec2d& size,
    bool selected) {
  auto& cached = cache_[nodeId];
  cached.vertices = vertices;
  cached.cachedPosition = position;
  cached.cachedSize = size;
  cached.cachedSelected = selected;
  cached.dirty = false;
  cached.generation = globalGeneration_;
}

const std::vector<NodeVertex>& NodeVertexCache::getVertices(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.vertices;
  }
  return emptyVertices();
}

uint32_t NodeVertexCache::getGeneration(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.generation;
  }
  return 0;
}

bool NodeVertexCache::updateSelectedFlag(
    const std::string& nodeId,
    bool selected,
    float innerStrokeValue) {
  auto it = cache_.find(nodeId);
  if (it == cache_.end()) {
    return false;
  }
  auto& cached = it->second;
  if (cached.cachedSelected == selected) {
    return false;
  }
  float selectedFloat = selected ? 1.0f : 0.0f;
  for (auto& vertex : cached.vertices) {
    vertex.selected = selectedFloat;
    vertex.innerStroke = innerStrokeValue;
  }
  cached.cachedSelected = selected;
  return true;
}

void NodeVertexCache::invalidateNode(const std::string& nodeId) {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    it->second.dirty = true;
  }
}

void NodeVertexCache::invalidateAll() {
  ++globalGeneration_;
}

void NodeVertexCache::removeNode(const std::string& nodeId) {
  cache_.erase(nodeId);
}

void NodeVertexCache::clear() {
  cache_.clear();
  globalGeneration_ = 0;
}

bool NodeVertexCache::contains(const std::string& nodeId) const {
  return cache_.find(nodeId) != cache_.end();
}

} // namespace noodles
