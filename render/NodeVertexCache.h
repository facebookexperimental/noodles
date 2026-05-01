// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_NODE_VERTEX_CACHE_H
#define NOODLES_RENDER_NODE_VERTEX_CACHE_H

#include "core/Math.h"
#include "core/NodeVertex.h"
#include "core/api.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

struct NOODLES_API CachedNodeData {
  std::vector<NodeVertex> vertices;
  uint32_t generation = 0;
  Vec2d cachedPosition{0.0, 0.0};
  Vec2d cachedSize{0.0, 0.0};
  bool cachedSelected = false;
  bool dirty = true;
};

class NOODLES_API NodeVertexCache {
 public:
  /// Check if a node needs regeneration.
  bool isDirty(const std::string& nodeId, const Vec2d& position, const Vec2d& size) const;

  /// Store generated vertices for a node.
  void update(
      const std::string& nodeId,
      const std::vector<NodeVertex>& vertices,
      const Vec2d& position,
      const Vec2d& size,
      bool selected);

  /// Get cached vertices (empty if not cached).
  const std::vector<NodeVertex>& getVertices(const std::string& nodeId) const;

  /// Get per-node generation counter.
  uint32_t getGeneration(const std::string& nodeId) const;

  /// Fast path: flip selected flag in cached vertices without full regeneration.
  bool updateSelectedFlag(const std::string& nodeId, bool selected, float innerStrokeValue);

  /// Mark a specific node for regeneration.
  void invalidateNode(const std::string& nodeId);

  /// Mark all nodes for regeneration.
  void invalidateAll();

  /// Remove a node from cache.
  void removeNode(const std::string& nodeId);

  /// Clear entire cache.
  void clear();

  /// Number of cached nodes.
  size_t size() const {
    return cache_.size();
  }

  /// Check if node is cached.
  bool contains(const std::string& nodeId) const;

 private:
  std::unordered_map<std::string, CachedNodeData> cache_;
  uint32_t globalGeneration_ = 0;
  static const std::vector<NodeVertex>& emptyVertices() {
    static const std::vector<NodeVertex> instance;
    return instance;
  }
};

} // namespace noodles
#endif // NOODLES_RENDER_NODE_VERTEX_CACHE_H
