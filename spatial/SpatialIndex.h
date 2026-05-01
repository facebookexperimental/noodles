// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_SPATIAL_SPATIAL_INDEX_H
#define NOODLES_SPATIAL_SPATIAL_INDEX_H

#include "core/Math.h"
#include "core/NodeData.h"
#include "core/api.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

/// Item types for spatial index
enum class SpatialItemType : int { Node = 0, Link = 1 };

/// \class SpatialItem
/// \brief Wrapper for items stored in the spatial index.
///
/// Stores a reference ID, bounding box, and type discrimination.
/// This is used internally by the quadtree structure.
class NOODLES_API SpatialItem {
 public:
  SpatialItem(SpatialItemType type, const std::string& id, const Range2d& bounds)
      : itemType(type), itemId(id), bounds(bounds) {}

  SpatialItemType itemType;
  std::string itemId; // Node ID (string) or link index as string
  Range2d bounds;
};

/// \class QuadtreeNode
/// \brief A single node in the quadtree spatial hierarchy.
///
/// Each node represents a rectangular region of 2D space and either:
/// - Contains items (leaf node)
/// - Has 4 children subdividing the space (internal node)
///
/// Children are indexed:
///   0: NW (top-left)
///   1: NE (top-right)
///   2: SW (bottom-left)
///   3: SE (bottom-right)
class NOODLES_API QuadtreeNode {
 public:
  /// Constructor
  /// \param bounds Bounding box for this node
  /// \param depth Current depth in the tree (0 = root)
  /// \param maxDepth Maximum allowed depth before stopping subdivision
  /// \param maxItems Maximum items in a leaf before subdividing
  /// \param minCellSize Minimum cell size to prevent tiny subdivisions
  explicit QuadtreeNode(
      const Range2d& bounds,
      int depth = 0,
      int maxDepth = 8,
      int maxItems = 10,
      double minCellSize = 10.0);

  /// Insert an item into this node or its children
  void Insert(const SpatialItem& item);

  /// Remove an item from this node or its children
  /// \return True if item was found and removed
  bool Remove(const SpatialItem& item);

  /// Query for items containing a point
  /// \param point Point to query
  /// \param results Vector to append matching SpatialItems to
  void QueryPoint(const Vec2d& point, std::vector<SpatialItem>& results) const;

  /// Query for items intersecting a region
  /// \param queryBounds Region to query
  /// \param results Vector to append matching SpatialItems to
  void QueryRegion(const Range2d& queryBounds, std::vector<SpatialItem>& results) const;

  /// Clear all items and children from this node
  void Clear();

  /// Get all cell bounding boxes in the tree (for debug visualization)
  void GetCells(std::vector<Range2d>& cells) const;

  /// Get the bounds of this node
  const Range2d& GetBounds() const {
    return _bounds;
  }

  /// Get items stored at this node level
  const std::vector<SpatialItem>& GetItems() const {
    return _items;
  }

  /// Check if this is a leaf node
  bool IsLeaf() const {
    return _isLeaf;
  }

 private:
  /// Subdivide this node into 4 children
  void _Subdivide();

  /// Insert an item into the appropriate child nodes
  void _InsertIntoChildren(const SpatialItem& item);

  Range2d _bounds;
  int _depth;
  int _maxDepth;
  int _maxItems;
  double _minCellSize;

  std::vector<SpatialItem> _items;
  std::array<std::unique_ptr<QuadtreeNode>, 4> _children;
  bool _isLeaf;
};

/// \class Quadtree
/// \brief Quadtree spatial data structure for 2D spatial partitioning.
///
class NOODLES_API Quadtree {
 public:
  Quadtree(
      const Range2d& bounds = Range2d(),
      int maxDepth = 8,
      int maxItems = 10,
      double minCellSize = 10.0);

  void Insert(const SpatialItem& item);
  bool Remove(const SpatialItem& item);

  std::vector<SpatialItem> QueryPoint(const Vec2d& point) const;
  std::vector<SpatialItem> QueryRegion(const Range2d& bounds) const;

  void Rebuild(const std::vector<SpatialItem>& items);
  void Clear();

  std::vector<Range2d> GetCells() const;
  const Range2d& GetBounds() const {
    return _bounds;
  }
  bool HasRoot() const {
    return _root != nullptr;
  }

 private:
  void _EnsureRoot(const Range2d& bounds = Range2d());
  void _ExpandBoundsIfNeeded(const Range2d& itemBounds);

  std::unique_ptr<QuadtreeNode> _root;
  Range2d _bounds;
  int _maxDepth;
  int _maxItems;
  double _minCellSize;
  std::vector<SpatialItem> _allItems;
};

/// \class SpatialIndex
/// \brief High-level spatial index for managing nodes and links in a node graph.
///
class NOODLES_API SpatialIndex {
 public:
  SpatialIndex(
      const Range2d& bounds = Range2d(),
      int maxDepth = 8,
      int maxItems = 10,
      double minCellSize = 10.0);

  void InsertNode(const std::string& nodeId, const Range2d& bounds);
  void InsertLink(int linkIndex, const Range2d& bounds);

  bool RemoveNode(const std::string& nodeId);
  bool RemoveLink(int linkIndex);

  void UpdateNode(const std::string& nodeId, const Range2d& bounds);

  void QueryPoint(
      const Vec2d& point,
      std::vector<std::string>& nodeIds,
      std::vector<int>& linkIndices) const;
  void QueryRegion(
      const Range2d& bounds,
      std::vector<std::string>& nodeIds,
      std::vector<int>& linkIndices) const;

  void BulkInsert(
      const std::vector<std::string>& nodeIds,
      const std::vector<Range2d>& nodeBounds,
      const std::vector<Range2d>& linkBounds);
  void Clear();

  std::vector<Range2d> GetCells() const;
  size_t GetNodeCount() const {
    return _nodeItems.size();
  }
  size_t GetLinkCount() const {
    return _linkItems.size();
  }

  static Range2d ComputeNodeBounds(const NodeData& node);
  static Range2d ComputeLinkBounds(const LinkData& link);

 private:
  static std::string _LinkIndexToId(int index);
  static int _IdToLinkIndex(const std::string& id);

  Quadtree _quadtree;

  std::unordered_map<std::string, SpatialItem> _nodeItems;
  std::unordered_map<int, SpatialItem> _linkItems;
};

} // namespace noodles

#endif // NOODLES_SPATIAL_SPATIAL_INDEX_H
