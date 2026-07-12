// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/TextVertexCache.h"

#include "core/NodeData.h"
#include "core/RenderConfig.h"

namespace noodles {

bool TextLayoutSignature::operator==(const TextLayoutSignature& other) const {
  return name == other.name && type == other.type && schemaTypeName == other.schemaTypeName &&
      size == other.size && inputPins == other.inputPins && outputPins == other.outputPins &&
      inputPinTypes == other.inputPinTypes && outputPinTypes == other.outputPinTypes &&
      inputRowKinds == other.inputRowKinds && outputRowKinds == other.outputRowKinds &&
      inputRowSlots == other.inputRowSlots && outputRowSlots == other.outputRowSlots &&
      relationshipOutputPins == other.relationshipOutputPins &&
      titleCollapsed == other.titleCollapsed && isGraffi == other.isGraffi &&
      showPinTypeLabels == other.showPinTypeLabels && titleFontSize == other.titleFontSize &&
      pinFontSize == other.pinFontSize && pinTypeFontSize == other.pinTypeFontSize &&
      marginH == other.marginH && marginV == other.marginV && portSpacing == other.portSpacing &&
      portWidth == other.portWidth;
}

TextLayoutSignature makeTextLayoutSignature(const NodeData& node, const RenderConfig& config) {
  TextLayoutSignature sig;
  sig.name = node.name;
  sig.type = node.type;
  sig.schemaTypeName = node.schemaTypeName;
  sig.size = node.size;
  sig.inputPins = node.inputPins;
  sig.outputPins = node.outputPins;
  sig.inputPinTypes = node.inputPinTypes;
  sig.outputPinTypes = node.outputPinTypes;
  sig.inputRowKinds = node.inputRowKinds;
  sig.outputRowKinds = node.outputRowKinds;
  sig.inputRowSlots = node.inputRowSlots;
  sig.outputRowSlots = node.outputRowSlots;
  sig.relationshipOutputPins = node.relationshipOutputPins;
  sig.titleCollapsed = node.titleCollapsed;
  sig.isGraffi = config.isGraffiStyle;
  sig.showPinTypeLabels = config.showPinTypeLabels;
  sig.titleFontSize = config.get("nodeTitleFontSize", 24.0);
  sig.pinFontSize = config.get("nodePinFontSize", 18.0);
  sig.pinTypeFontSize = config.get("nodePinTypeFontSize", 14.0);
  sig.marginH = config.get("nodeMarginH", 16.0);
  sig.marginV = config.get("nodeMarginV", 18.0);
  sig.portSpacing = config.get("nodePortSpacing", 1.0);
  sig.portWidth = config.get("nodePortWidth", 16.0);
  return sig;
}

bool TextVertexCache::isDirty(const std::string& nodeId, const TextLayoutSignature& signature)
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

void TextVertexCache::update(
    const std::string& nodeId,
    std::vector<float> vertices,
    const TextLayoutSignature& signature) {
  auto& cached = cache_[nodeId];
  cached.vertices = std::move(vertices);
  cached.signature = signature;
  cached.dirty = false;
  cached.generation = globalGeneration_;
}

const std::vector<float>& TextVertexCache::getVertices(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.vertices;
  }
  return emptyVertices();
}

uint32_t TextVertexCache::getGeneration(const std::string& nodeId) const {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    return it->second.generation;
  }
  return 0;
}

void TextVertexCache::invalidateNode(const std::string& nodeId) {
  auto it = cache_.find(nodeId);
  if (it != cache_.end()) {
    it->second.dirty = true;
  }
}

void TextVertexCache::invalidateAll() {
  ++globalGeneration_;
}

void TextVertexCache::removeNode(const std::string& nodeId) {
  cache_.erase(nodeId);
}

void TextVertexCache::retainNodes(const std::unordered_set<std::string>& liveIds) {
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (liveIds.find(it->first) == liveIds.end()) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void TextVertexCache::clear() {
  cache_.clear();
  globalGeneration_ = 0;
}

bool TextVertexCache::contains(const std::string& nodeId) const {
  return cache_.find(nodeId) != cache_.end();
}

const std::vector<float>& TextVertexCache::emptyVertices() {
  static const std::vector<float> instance;
  return instance;
}

} // namespace noodles
