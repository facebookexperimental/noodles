// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/IconVertexCache.h"

#include "core/NodeData.h"
#include "core/RenderConfig.h"

namespace noodles {

bool IconLayoutSignature::operator==(const IconLayoutSignature& other) const {
  return titleIconPath == other.titleIconPath && titleCollapsed == other.titleCollapsed &&
      inputPins == other.inputPins && outputPins == other.outputPins &&
      inputRowKinds == other.inputRowKinds && outputRowKinds == other.outputRowKinds &&
      inputRowSlots == other.inputRowSlots && outputRowSlots == other.outputRowSlots &&
      relationshipInputPins == other.relationshipInputPins &&
      relationshipOutputPins == other.relationshipOutputPins && titleHeight == other.titleHeight &&
      portStartY == other.portStartY && portLineHeight == other.portLineHeight &&
      titleFontSize == other.titleFontSize && marginH == other.marginH;
}

IconLayoutSignature makeIconLayoutSignature(const NodeData& node, const RenderConfig& config) {
  IconLayoutSignature sig;
  sig.titleIconPath = node.titleIconPath;
  sig.titleCollapsed = node.titleCollapsed;
  sig.inputPins = node.inputPins;
  sig.outputPins = node.outputPins;
  sig.inputRowKinds = node.inputRowKinds;
  sig.outputRowKinds = node.outputRowKinds;
  sig.inputRowSlots = node.inputRowSlots;
  sig.outputRowSlots = node.outputRowSlots;
  sig.relationshipInputPins = node.relationshipInputPins;
  sig.relationshipOutputPins = node.relationshipOutputPins;
  sig.titleHeight = node.layoutTitleHeight;
  sig.portStartY = node.layoutPortStartY;
  sig.portLineHeight = node.layoutPortLineHeight;
  sig.titleFontSize = config.get("nodeTitleFontSize", 24.0);
  sig.marginH = config.get("nodeMarginH", 16.0);
  return sig;
}

bool IconVertexCache::isDirty(const std::string& nodeId, const IconLayoutSignature& signature)
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
  return cached.signature != signature;
}

void IconVertexCache::update(
    const std::string& nodeId,
    std::vector<float> vertices,
    std::vector<std::string> quadTextureIds,
    const IconLayoutSignature& signature) {
  auto& cached = cache_[nodeId];
  cached.vertices = std::move(vertices);
  cached.quadTextureIds = std::move(quadTextureIds);
  cached.signature = signature;
  cached.dirty = false;
  cached.generation = globalGeneration_;
}

const std::vector<float>& IconVertexCache::getVertices(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.vertices;
  }
  return emptyVertices();
}

const std::vector<std::string>& IconVertexCache::getTextureIds(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.quadTextureIds;
  }
  return emptyTextureIds();
}

uint32_t IconVertexCache::getGeneration(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.generation;
  }
  return 0;
}

void IconVertexCache::invalidateNode(const std::string& nodeId) {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    it->second.dirty = true;
  }
}

void IconVertexCache::invalidateAll() {
  ++globalGeneration_;
}

void IconVertexCache::removeNode(const std::string& nodeId) {
  cache_.erase(nodeId);
}

void IconVertexCache::retainNodes(const std::unordered_set<std::string>& liveIds) {
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (liveIds.find(it->first) == liveIds.end()) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void IconVertexCache::clear() {
  cache_.clear();
  globalGeneration_ = 0;
}

bool IconVertexCache::contains(const std::string& nodeId) const {
  return cache_.find(nodeId) != cache_.end();
}

const std::vector<float>& IconVertexCache::emptyVertices() {
  static const std::vector<float> instance;
  return instance;
}

const std::vector<std::string>& IconVertexCache::emptyTextureIds() {
  static const std::vector<std::string> instance;
  return instance;
}

} // namespace noodles
