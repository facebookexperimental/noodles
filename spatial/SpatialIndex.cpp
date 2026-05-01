// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "spatial/SpatialIndex.h"

#include <algorithm>
#include <cmath>

namespace noodles {

QuadtreeNode::QuadtreeNode(
    const Range2d& bounds,
    int depth,
    int maxDepth,
    int maxItems,
    double minCellSize)
    : _bounds(bounds),
      _depth(depth),
      _maxDepth(maxDepth),
      _maxItems(maxItems),
      _minCellSize(minCellSize),
      _isLeaf(true) {}

void QuadtreeNode::_Subdivide() {
  if (!_isLeaf) {
    return; // Already subdivided
  }

  Vec2d size = _bounds.GetSize();
  if (_depth >= _maxDepth || size[0] < _minCellSize * 2 || size[1] < _minCellSize * 2) {
    return;
  }

  // Create 4 children
  Vec2d minPt = _bounds.GetMin();
  Vec2d maxPt = _bounds.GetMax();
  double midX = (minPt[0] + maxPt[0]) * 0.5;
  double midY = (minPt[1] + maxPt[1]) * 0.5;
  Vec2d mid(midX, midY);

  // NW (top-left) - remember Y increases downward in screen space
  _children[0] = std::make_unique<QuadtreeNode>(
      Range2d(minPt, mid), _depth + 1, _maxDepth, _maxItems, _minCellSize);

  // NE (top-right)
  _children[1] = std::make_unique<QuadtreeNode>(
      Range2d(Vec2d(midX, minPt[1]), Vec2d(maxPt[0], midY)),
      _depth + 1,
      _maxDepth,
      _maxItems,
      _minCellSize);

  // SW (bottom-left)
  _children[2] = std::make_unique<QuadtreeNode>(
      Range2d(Vec2d(minPt[0], midY), Vec2d(midX, maxPt[1])),
      _depth + 1,
      _maxDepth,
      _maxItems,
      _minCellSize);

  // SE (bottom-right)
  _children[3] = std::make_unique<QuadtreeNode>(
      Range2d(mid, maxPt), _depth + 1, _maxDepth, _maxItems, _minCellSize);

  _isLeaf = false;

  // Redistribute items to children
  // Keep large items that overlap too many children at this level
  std::vector<SpatialItem> itemsToRedistribute = std::move(_items);
  _items.clear();

  for (const SpatialItem& item : itemsToRedistribute) {
    // Count how many children this item overlaps
    int overlappingChildren = 0;
    QuadtreeNode* overlappingChild = nullptr;

    for (int i = 0; i < 4; ++i) {
      Range2d intersection = Range2d::GetIntersection(_children[i]->_bounds, item.bounds);
      if (!intersection.IsEmpty()) {
        overlappingChildren++;
        overlappingChild = _children[i].get();
      }
    }

    // CRITICAL: Only push down items that fit entirely within ONE child
    // If item overlaps 2+ children, keep it at this level to prevent duplication
    if (overlappingChildren == 1 && overlappingChild) {
      overlappingChild->Insert(item);
    } else {
      _items.push_back(item);
    }
  }
}

void QuadtreeNode::_InsertIntoChildren(const SpatialItem& item) {
  if (_isLeaf) {
    return;
  }

  int overlappingChildren = 0;
  QuadtreeNode* overlappingChild = nullptr;

  for (int i = 0; i < 4; ++i) {
    Range2d intersection = Range2d::GetIntersection(_children[i]->_bounds, item.bounds);
    if (!intersection.IsEmpty()) {
      overlappingChildren++;
      overlappingChild = _children[i].get();
    }
  }

  if (overlappingChildren == 1 && overlappingChild) {
    overlappingChild->Insert(item);
  } else {
    _items.push_back(item);
  }
}

void QuadtreeNode::Insert(const SpatialItem& item) {
  if (!_isLeaf) {
    _InsertIntoChildren(item);
    return;
  }

  _items.push_back(item);

  if (static_cast<int>(_items.size()) > _maxItems) {
    _Subdivide();
  }
}

bool QuadtreeNode::Remove(const SpatialItem& item) {
  // Try to remove from this level first
  for (auto it = _items.begin(); it != _items.end(); ++it) {
    if (it->itemType == item.itemType && it->itemId == item.itemId) {
      _items.erase(it);
      return true;
    }
  }

  // If not a leaf, also try children
  if (!_isLeaf) {
    bool removed = false;
    for (int i = 0; i < 4; ++i) {
      Range2d intersection = Range2d::GetIntersection(_children[i]->_bounds, item.bounds);
      if (!intersection.IsEmpty()) {
        if (_children[i]->Remove(item)) {
          removed = true;
        }
      }
    }
    return removed;
  }

  return false;
}

void QuadtreeNode::QueryPoint(const Vec2d& point, std::vector<SpatialItem>& results) const {
  if (!_bounds.Contains(point)) {
    return;
  }

  // Check items at this level
  for (const SpatialItem& item : _items) {
    if (item.bounds.Contains(point)) {
      results.push_back(item);
    }
  }

  // If not a leaf, also recurse into children
  if (!_isLeaf) {
    for (int i = 0; i < 4; ++i) {
      _children[i]->QueryPoint(point, results);
    }
  }
}

void QuadtreeNode::QueryRegion(const Range2d& queryBounds, std::vector<SpatialItem>& results)
    const {
  Range2d intersection = Range2d::GetIntersection(_bounds, queryBounds);
  if (intersection.IsEmpty()) {
    return;
  }

  // Check items at this level
  for (const SpatialItem& item : _items) {
    Range2d itemIntersection = Range2d::GetIntersection(item.bounds, queryBounds);
    if (!itemIntersection.IsEmpty()) {
      results.push_back(item);
    }
  }

  // If not a leaf, also recurse into children
  if (!_isLeaf) {
    for (int i = 0; i < 4; ++i) {
      _children[i]->QueryRegion(queryBounds, results);
    }
  }
}

void QuadtreeNode::Clear() {
  _items.clear();
  for (int i = 0; i < 4; ++i) {
    _children[i].reset();
  }
  _isLeaf = true;
}

void QuadtreeNode::GetCells(std::vector<Range2d>& cells) const {
  cells.push_back(_bounds);

  if (!_isLeaf) {
    for (int i = 0; i < 4; ++i) {
      if (_children[i]) {
        _children[i]->GetCells(cells);
      }
    }
  }
}

//
// Quadtree Implementation
//

Quadtree::Quadtree(const Range2d& bounds, int maxDepth, int maxItems, double minCellSize)
    : _bounds(bounds), _maxDepth(maxDepth), _maxItems(maxItems), _minCellSize(minCellSize) {}

void Quadtree::_EnsureRoot(const Range2d& bounds) {
  if (_root) {
    return;
  }

  Range2d rootBounds = bounds;
  if (rootBounds.IsEmpty()) {
    rootBounds = _bounds;
  }
  if (rootBounds.IsEmpty()) {
    // Default bounds if nothing provided
    rootBounds = Range2d(Vec2d(-10000, -10000), Vec2d(10000, 10000));
  }

  _bounds = rootBounds;
  _root = std::make_unique<QuadtreeNode>(_bounds, 0, _maxDepth, _maxItems, _minCellSize);
}

void Quadtree::_ExpandBoundsIfNeeded(const Range2d& itemBounds) {
  if (!_root) {
    return;
  }

  // Check if expansion is needed
  Range2d intersection = Range2d::GetIntersection(_root->GetBounds(), itemBounds);
  if (intersection == itemBounds) {
    // Item fully contained - no expansion needed
    return;
  }

  // Calculate new expanded bounds
  Range2d newBounds = Range2d::GetUnion(_root->GetBounds(), itemBounds);

  // Add 10% padding to the expansion
  Vec2d size = newBounds.GetSize();
  Vec2d padding(size[0] * 0.1, size[1] * 0.1);
  newBounds = Range2d(newBounds.GetMin() - padding, newBounds.GetMax() + padding);

  _bounds = newBounds;

  // Rebuild the tree with new bounds (simpler than recursive expansion)
  std::vector<SpatialItem> allItems = std::move(_allItems);
  _allItems.clear();
  _root.reset();
  _EnsureRoot(newBounds);

  for (const SpatialItem& item : allItems) {
    _root->Insert(item);
    _allItems.push_back(item);
  }
}

void Quadtree::Insert(const SpatialItem& item) {
  _EnsureRoot();
  _ExpandBoundsIfNeeded(item.bounds);
  _root->Insert(item);
  _allItems.push_back(item);
}

bool Quadtree::Remove(const SpatialItem& item) {
  if (!_root) {
    return false;
  }

  bool removed = _root->Remove(item);
  if (removed) {
    // Remove from allItems as well
    auto it =
        std::find_if(_allItems.begin(), _allItems.end(), [&item](const SpatialItem& existing) {
          return existing.itemType == item.itemType && existing.itemId == item.itemId;
        });
    if (it != _allItems.end()) {
      _allItems.erase(it);
    }
  }
  return removed;
}

std::vector<SpatialItem> Quadtree::QueryPoint(const Vec2d& point) const {
  std::vector<SpatialItem> results;
  if (_root) {
    _root->QueryPoint(point, results);
  }
  return results;
}

std::vector<SpatialItem> Quadtree::QueryRegion(const Range2d& bounds) const {
  std::vector<SpatialItem> results;
  if (_root) {
    _root->QueryRegion(bounds, results);
  }
  return results;
}

void Quadtree::Rebuild(const std::vector<SpatialItem>& items) {
  Clear();

  if (items.empty()) {
    return;
  }

  // Calculate bounds from ALL items
  Range2d calculatedBounds = items[0].bounds;
  for (size_t i = 1; i < items.size(); ++i) {
    calculatedBounds = Range2d::GetUnion(calculatedBounds, items[i].bounds);
  }

  // Add 10% padding
  Vec2d size = calculatedBounds.GetSize();
  Vec2d padding(size[0] * 0.1, size[1] * 0.1);
  calculatedBounds =
      Range2d(calculatedBounds.GetMin() - padding, calculatedBounds.GetMax() + padding);

  // Create root with calculated bounds
  _bounds = calculatedBounds;
  _EnsureRoot(calculatedBounds);

  // Insert all items
  for (const SpatialItem& item : items) {
    _root->Insert(item);
    _allItems.push_back(item);
  }
}

void Quadtree::Clear() {
  _root.reset();
  _allItems.clear();
}

std::vector<Range2d> Quadtree::GetCells() const {
  std::vector<Range2d> cells;
  if (_root) {
    _root->GetCells(cells);
  }
  return cells;
}

//
// SpatialIndex Implementation
//

SpatialIndex::SpatialIndex(const Range2d& bounds, int maxDepth, int maxItems, double minCellSize)
    : _quadtree(bounds, maxDepth, maxItems, minCellSize) {}

std::string SpatialIndex::_LinkIndexToId(int index) {
  return "link_" + std::to_string(index);
}

int SpatialIndex::_IdToLinkIndex(const std::string& id) {
  // Parse "link_N" format
  if (id.size() > 5 && id.substr(0, 5) == "link_") {
    return std::stoi(id.substr(5));
  }
  return -1;
}

void SpatialIndex::InsertNode(const std::string& nodeId, const Range2d& bounds) {
  RemoveNode(nodeId);

  SpatialItem item(SpatialItemType::Node, nodeId, bounds);
  _quadtree.Insert(item);
  _nodeItems.emplace(nodeId, item);
}

void SpatialIndex::InsertLink(int linkIndex, const Range2d& bounds) {
  RemoveLink(linkIndex);

  std::string itemId = _LinkIndexToId(linkIndex);
  SpatialItem item(SpatialItemType::Link, itemId, bounds);
  _quadtree.Insert(item);
  _linkItems.emplace(linkIndex, item);
}

bool SpatialIndex::RemoveNode(const std::string& nodeId) {
  auto it = _nodeItems.find(nodeId);
  if (it == _nodeItems.end()) {
    return false;
  }

  _quadtree.Remove(it->second);
  _nodeItems.erase(it);
  return true;
}

bool SpatialIndex::RemoveLink(int linkIndex) {
  auto it = _linkItems.find(linkIndex);
  if (it == _linkItems.end()) {
    return false;
  }

  _quadtree.Remove(it->second);
  _linkItems.erase(it);
  return true;
}

void SpatialIndex::UpdateNode(const std::string& nodeId, const Range2d& bounds) {
  RemoveNode(nodeId);
  InsertNode(nodeId, bounds);
}

void SpatialIndex::QueryPoint(
    const Vec2d& point,
    std::vector<std::string>& nodeIds,
    std::vector<int>& linkIndices) const {
  std::vector<SpatialItem> items = _quadtree.QueryPoint(point);

  for (const SpatialItem& item : items) {
    if (item.itemType == SpatialItemType::Node) {
      nodeIds.push_back(item.itemId);
    } else {
      int linkIndex = _IdToLinkIndex(item.itemId);
      if (linkIndex >= 0) {
        linkIndices.push_back(linkIndex);
      }
    }
  }
}

void SpatialIndex::QueryRegion(
    const Range2d& bounds,
    std::vector<std::string>& nodeIds,
    std::vector<int>& linkIndices) const {
  std::vector<SpatialItem> items = _quadtree.QueryRegion(bounds);

  for (const SpatialItem& item : items) {
    if (item.itemType == SpatialItemType::Node) {
      nodeIds.push_back(item.itemId);
    } else {
      int linkIndex = _IdToLinkIndex(item.itemId);
      if (linkIndex >= 0) {
        linkIndices.push_back(linkIndex);
      }
    }
  }
}

void SpatialIndex::BulkInsert(
    const std::vector<std::string>& nodeIds,
    const std::vector<Range2d>& nodeBounds,
    const std::vector<Range2d>& linkBounds) {
  Clear();

  // Build all items
  std::vector<SpatialItem> allItems;
  allItems.reserve(nodeIds.size() + linkBounds.size());

  // Add node items
  for (size_t i = 0; i < nodeIds.size() && i < nodeBounds.size(); ++i) {
    SpatialItem item(SpatialItemType::Node, nodeIds[i], nodeBounds[i]);
    allItems.push_back(item);
    _nodeItems.emplace(nodeIds[i], item);
  }

  // Add link items
  for (size_t i = 0; i < linkBounds.size(); ++i) {
    std::string itemId = _LinkIndexToId(static_cast<int>(i));
    SpatialItem item(SpatialItemType::Link, itemId, linkBounds[i]);
    allItems.push_back(item);
    _linkItems.emplace(static_cast<int>(i), item);
  }

  // Rebuild quadtree with all items
  _quadtree.Rebuild(allItems);
}

void SpatialIndex::Clear() {
  _quadtree.Clear();
  _nodeItems.clear();
  _linkItems.clear();
}

std::vector<Range2d> SpatialIndex::GetCells() const {
  return _quadtree.GetCells();
}

Range2d SpatialIndex::ComputeNodeBounds(const NodeData& node) {
  return Range2d(node.position, node.position + node.size);
}

Range2d SpatialIndex::ComputeLinkBounds(const LinkData& link) {
  double minX = std::min(link.start[0], link.end[0]);
  double maxX = std::max(link.start[0], link.end[0]);
  double minY = std::min(link.start[1], link.end[1]);
  double maxY = std::max(link.start[1], link.end[1]);

  double dx = std::abs(link.end[0] - link.start[0]);
  double dy = std::abs(link.end[1] - link.start[1]);
  double margin = std::max(dx, dy) * 0.3;

  return Range2d(Vec2d(minX - margin, minY - margin), Vec2d(maxX + margin, maxY + margin));
}

} // namespace noodles
