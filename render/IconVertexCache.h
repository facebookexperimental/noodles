// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_ICON_VERTEX_CACHE_H
#define NOODLES_RENDER_ICON_VERTEX_CACHE_H

#include "core/api.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noodles {

struct NodeData;
struct RenderConfig;

/// Every field buildNodeIconQuads reads must appear here, or edits to that field
/// silently render stale icons. Position is intentionally excluded: a node's
/// icons bake at its base frame and ride the live move offset via the transform
/// texture, so a move must NOT force re-layout (mirrors TextLayoutSignature).
struct NOODLES_API IconLayoutSignature {
  // Title-bar icon path (drives which texture the title icon samples).
  std::string titleIconPath;
  bool titleCollapsed = false;
  // Pin order + row kinds/slots place each row icon and decide which rows show
  // one (rowKind 1/2 are fold carets and get no icon).
  std::vector<std::string> inputPins;
  std::vector<std::string> outputPins;
  std::vector<int> inputRowKinds;
  std::vector<int> outputRowKinds;
  std::vector<int> inputRowSlots;
  std::vector<int> outputRowSlots;
  // A row is a relationship row (double-arrow icon vs minus) if its pin is in
  // either set, so both must invalidate the cached icons.
  std::unordered_set<std::string> relationshipInputPins;
  std::unordered_set<std::string> relationshipOutputPins;
  // Cached layout metrics that fix icon placement.
  double titleHeight = 0.0;
  double portStartY = 0.0;
  double portLineHeight = 0.0;
  // Config metrics — a font-size / margin change re-lays-out every node.
  double titleFontSize = 0.0;
  double marginH = 0.0;

  bool operator==(const IconLayoutSignature& other) const;
  bool operator!=(const IconLayoutSignature& other) const {
    return !(*this == other);
  }
};

/// Keep in sync with buildNodeIconQuads.
NOODLES_API IconLayoutSignature
makeIconLayoutSignature(const NodeData& node, const RenderConfig& config);

struct NOODLES_API CachedNodeIconData {
  std::vector<float> vertices;
  // One texture id per quad (each quad is 6 vertices in `vertices`), parallel to
  // the quad order. Icons span several textures (title icon / minus /
  // relationship-arrows), so the draw groups by this; text needs no equivalent.
  std::vector<std::string> quadTextureIds;
  IconLayoutSignature signature;
  uint32_t generation = 0;
  bool dirty = true;
};

/// Per-node icon vertex cache (mirrors TextVertexCache). GL/FontAtlas-free so it
/// can be unit-tested without a graphics context.
class NOODLES_API IconVertexCache {
 public:
  bool isDirty(const std::string& nodeId, const IconLayoutSignature& signature) const;
  void update(
      const std::string& nodeId,
      std::vector<float> vertices,
      std::vector<std::string> quadTextureIds,
      const IconLayoutSignature& signature);
  const std::vector<float>& getVertices(const std::string& nodeId) const;
  const std::vector<std::string>& getTextureIds(const std::string& nodeId) const;
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
  std::unordered_map<std::string, CachedNodeIconData> cache_;
  uint32_t globalGeneration_ = 0;
  static const std::vector<float>& emptyVertices();
  static const std::vector<std::string>& emptyTextureIds();
};

} // namespace noodles

#endif // NOODLES_RENDER_ICON_VERTEX_CACHE_H
