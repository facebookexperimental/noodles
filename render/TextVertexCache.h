// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_TEXT_VERTEX_CACHE_H
#define NOODLES_RENDER_TEXT_VERTEX_CACHE_H

#include "core/Math.h"
#include "core/api.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noodles {

struct NodeData;
struct RenderConfig;

/// Every field that layoutSingleNode reads must appear here, or edits to that
/// field silently render stale text. zOrder/nodeIndex are excluded because
/// assembly re-bakes them — a z-order shift shouldn't force glyph re-layout.
struct NOODLES_API TextLayoutSignature {
  std::string name;
  std::string type;
  std::string schemaTypeName;
  // Only width matters for layout, but tracking both is safe (over-invalidation
  // is harmless, under-invalidation is not).
  Vec2d size{0.0, 0.0};
  std::vector<std::string> inputPins;
  std::vector<std::string> outputPins;
  std::unordered_map<std::string, std::string> inputPinTypes;
  std::unordered_map<std::string, std::string> outputPinTypes;
  std::vector<int> inputRowKinds;
  std::vector<int> outputRowKinds;
  std::vector<int> inputRowSlots;
  std::vector<int> outputRowSlots;
  // Only output rows reserve a right-margin icon, so inputs don't matter here.
  std::unordered_set<std::string> relationshipOutputPins;
  bool titleCollapsed = false;
  bool isGraffi = false;
  // Toggling pin-type labels changes both layout (rows shrink) and which glyphs
  // are emitted, so cached text must invalidate when it flips.
  bool showPinTypeLabels = true;
  // Config metrics — a font-size or margin change needs to invalidate every node.
  double titleFontSize = 0.0;
  double pinFontSize = 0.0;
  double pinTypeFontSize = 0.0;
  double marginH = 0.0;
  double marginV = 0.0;
  double portSpacing = 0.0;
  double portWidth = 0.0;

  bool operator==(const TextLayoutSignature& other) const;
  bool operator!=(const TextLayoutSignature& other) const {
    return !(*this == other);
  }
};

/// Keep in sync with layoutSingleNode.
NOODLES_API TextLayoutSignature
makeTextLayoutSignature(const NodeData& node, const RenderConfig& config);

struct NOODLES_API CachedNodeTextData {
  std::vector<float> vertices;
  TextLayoutSignature signature;
  uint32_t generation = 0;
  bool dirty = true;
};

/// Per-node text vertex cache (mirrors NodeVertexCache for the quad path).
/// GL/FontAtlas-free so it can be unit-tested without a graphics context.
class NOODLES_API TextVertexCache {
 public:
  bool isDirty(const std::string& nodeId, const TextLayoutSignature& signature) const;
  void update(
      const std::string& nodeId,
      std::vector<float> vertices,
      const TextLayoutSignature& signature);
  const std::vector<float>& getVertices(const std::string& nodeId) const;
  uint32_t getGeneration(const std::string& nodeId) const;
  void invalidateNode(const std::string& nodeId);
  void invalidateAll();
  void removeNode(const std::string& nodeId);
  void retainNodes(const std::unordered_set<std::string>& liveIds);
  void clear();

  size_t size() const {
    return cache_.size();
  }

  bool contains(const std::string& nodeId) const;

 private:
  std::unordered_map<std::string, CachedNodeTextData> cache_;
  uint32_t globalGeneration_ = 0;
  static const std::vector<float>& emptyVertices();
};

} // namespace noodles

#endif // NOODLES_RENDER_TEXT_VERTEX_CACHE_H
