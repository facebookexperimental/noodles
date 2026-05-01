// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/NodeData.h"
#include "spatial/SpatialIndex.h"

#include <algorithm>
#include <unordered_set>

namespace noodles {

// ---------------------------------------------------------------------------
// QuadtreeNode
// ---------------------------------------------------------------------------

TEST(QuadtreeNodeTest, Construction) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds);

  EXPECT_TRUE(node.IsLeaf());
  EXPECT_TRUE(node.GetItems().empty());
  EXPECT_EQ(bounds, node.GetBounds());
}

TEST(QuadtreeNodeTest, InsertBelowThreshold) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20)));
  node.Insert(item);

  EXPECT_TRUE(node.IsLeaf());
  EXPECT_EQ(1u, node.GetItems().size());
}

TEST(QuadtreeNodeTest, SubdividesWhenFull) {
  Range2d bounds(Vec2d(0, 0), Vec2d(1000, 1000));
  QuadtreeNode node(bounds, 0, 8, 3); // maxItems=3

  // Insert 4 items in different quadrants to trigger subdivision
  node.Insert(SpatialItem(SpatialItemType::Node, "nw", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "ne", Range2d(Vec2d(600, 10), Vec2d(610, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "sw", Range2d(Vec2d(10, 600), Vec2d(20, 610))));
  node.Insert(SpatialItem(SpatialItemType::Node, "se", Range2d(Vec2d(600, 600), Vec2d(610, 610))));

  EXPECT_FALSE(node.IsLeaf());
}

TEST(QuadtreeNodeTest, QueryPointHit) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  node.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));

  std::vector<SpatialItem> results;
  node.QueryPoint(Vec2d(30, 30), results);
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ("n1", results[0].itemId);
}

TEST(QuadtreeNodeTest, QueryPointMiss) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  node.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));

  std::vector<SpatialItem> results;
  node.QueryPoint(Vec2d(80, 80), results);
  EXPECT_TRUE(results.empty());
}

TEST(QuadtreeNodeTest, QueryPointOutsideBounds) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  node.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));

  std::vector<SpatialItem> results;
  node.QueryPoint(Vec2d(200, 200), results);
  EXPECT_TRUE(results.empty());
}

TEST(QuadtreeNodeTest, QueryRegion) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  node.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(30, 30))));
  node.Insert(SpatialItem(SpatialItemType::Node, "n2", Range2d(Vec2d(50, 50), Vec2d(70, 70))));
  node.Insert(SpatialItem(SpatialItemType::Node, "n3", Range2d(Vec2d(80, 80), Vec2d(90, 90))));

  std::vector<SpatialItem> results;
  node.QueryRegion(Range2d(Vec2d(0, 0), Vec2d(60, 60)), results);
  EXPECT_EQ(2u, results.size());
}

TEST(QuadtreeNodeTest, Remove) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20)));
  node.Insert(item);
  EXPECT_EQ(1u, node.GetItems().size());

  EXPECT_TRUE(node.Remove(item));
  EXPECT_TRUE(node.GetItems().empty());
}

TEST(QuadtreeNodeTest, RemoveNonexistent) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20)));
  EXPECT_FALSE(node.Remove(item));
}

TEST(QuadtreeNodeTest, Clear) {
  Range2d bounds(Vec2d(0, 0), Vec2d(100, 100));
  QuadtreeNode node(bounds, 0, 8, 10);

  node.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "n2", Range2d(Vec2d(30, 30), Vec2d(40, 40))));

  node.Clear();
  EXPECT_TRUE(node.IsLeaf());
  EXPECT_TRUE(node.GetItems().empty());
}

TEST(QuadtreeNodeTest, GetCells) {
  Range2d bounds(Vec2d(0, 0), Vec2d(1000, 1000));
  QuadtreeNode node(bounds, 0, 8, 2);

  // Force subdivision by inserting 3 items in distinct quadrants
  node.Insert(SpatialItem(SpatialItemType::Node, "a", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "b", Range2d(Vec2d(600, 10), Vec2d(610, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "c", Range2d(Vec2d(10, 600), Vec2d(20, 610))));

  std::vector<Range2d> cells;
  node.GetCells(cells);
  // Root + 4 children = 5
  EXPECT_EQ(5u, cells.size());
}

TEST(QuadtreeNodeTest, MaxDepthPreventsSubdivision) {
  Range2d bounds(Vec2d(0, 0), Vec2d(1000, 1000));
  QuadtreeNode node(bounds, 0, 0, 1); // maxDepth=0

  // Even with maxItems=1, should not subdivide at maxDepth
  node.Insert(SpatialItem(SpatialItemType::Node, "a", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "b", Range2d(Vec2d(600, 10), Vec2d(610, 20))));

  EXPECT_TRUE(node.IsLeaf());
  EXPECT_EQ(2u, node.GetItems().size());
}

TEST(QuadtreeNodeTest, MinCellSizePreventsSubdivision) {
  Range2d bounds(Vec2d(0, 0), Vec2d(10, 10));
  QuadtreeNode node(bounds, 0, 8, 1, 10.0); // minCellSize = 10, cell is 10x10

  node.Insert(SpatialItem(SpatialItemType::Node, "a", Range2d(Vec2d(1, 1), Vec2d(2, 2))));
  node.Insert(SpatialItem(SpatialItemType::Node, "b", Range2d(Vec2d(6, 6), Vec2d(7, 7))));

  EXPECT_TRUE(node.IsLeaf());
}

// ---------------------------------------------------------------------------
// QuadtreeNode -- spanning item behavior
// ---------------------------------------------------------------------------

TEST(QuadtreeNodeTest, SpanningItemKeptAtParentDuringSubdivision) {
  Range2d bounds(Vec2d(0, 0), Vec2d(1000, 1000));
  QuadtreeNode node(bounds, 0, 8, 3);

  // Insert a spanning item before subdivision
  SpatialItem spanning(
      SpatialItemType::Node, "spanning", Range2d(Vec2d(400, 400), Vec2d(600, 600)));
  node.Insert(spanning);

  // Insert more items to trigger subdivision
  node.Insert(SpatialItem(SpatialItemType::Node, "nw", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "ne", Range2d(Vec2d(600, 10), Vec2d(610, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "se", Range2d(Vec2d(600, 600), Vec2d(610, 610))));

  EXPECT_FALSE(node.IsLeaf());

  // The spanning item should be kept at this level (not pushed to children)
  bool foundAtParent = false;
  for (const auto& item : node.GetItems()) {
    if (item.itemId == "spanning") {
      foundAtParent = true;
    }
  }
  EXPECT_TRUE(foundAtParent);

  // Query should find the spanning item exactly once
  std::vector<SpatialItem> results;
  node.QueryPoint(Vec2d(500, 500), results);
  int spanCount = 0;
  for (const auto& r : results) {
    if (r.itemId == "spanning") {
      spanCount++;
    }
  }
  EXPECT_EQ(1, spanCount);
}

TEST(QuadtreeNodeTest, PostSubdivisionSpanningInsertNoDuplicates) {
  // Verifies that items inserted after subdivision that span multiple
  // children are kept at the parent level, producing exactly one query result.
  Range2d bounds(Vec2d(0, 0), Vec2d(1000, 1000));
  QuadtreeNode node(bounds, 0, 8, 2);

  // Trigger subdivision with items in distinct quadrants
  node.Insert(SpatialItem(SpatialItemType::Node, "a", Range2d(Vec2d(10, 10), Vec2d(20, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "b", Range2d(Vec2d(600, 10), Vec2d(610, 20))));
  node.Insert(SpatialItem(SpatialItemType::Node, "c", Range2d(Vec2d(10, 600), Vec2d(20, 610))));
  EXPECT_FALSE(node.IsLeaf());

  // Insert a spanning item AFTER subdivision
  SpatialItem spanning(
      SpatialItemType::Node, "spanning", Range2d(Vec2d(400, 400), Vec2d(600, 600)));
  node.Insert(spanning);

  // Query the center -- spanning item should appear exactly once
  std::vector<SpatialItem> results;
  node.QueryPoint(Vec2d(500, 500), results);

  int spanCount = 0;
  for (const auto& r : results) {
    if (r.itemId == "spanning") {
      spanCount++;
    }
  }

  EXPECT_EQ(1, spanCount);
}

// ---------------------------------------------------------------------------
// Quadtree
// ---------------------------------------------------------------------------

TEST(QuadtreeTest, DefaultConstruction) {
  Quadtree tree;
  EXPECT_FALSE(tree.HasRoot());
}

TEST(QuadtreeTest, InsertCreatesRoot) {
  Quadtree tree;
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(0, 0), Vec2d(10, 10))));
  EXPECT_TRUE(tree.HasRoot());
}

TEST(QuadtreeTest, QueryPointAfterInsert) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n2", Range2d(Vec2d(60, 60), Vec2d(90, 90))));

  auto results = tree.QueryPoint(Vec2d(30, 30));
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ("n1", results[0].itemId);

  results = tree.QueryPoint(Vec2d(70, 70));
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ("n2", results[0].itemId);
}

TEST(QuadtreeTest, QueryRegionMultipleResults) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(30, 30))));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n2", Range2d(Vec2d(20, 20), Vec2d(40, 40))));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n3", Range2d(Vec2d(80, 80), Vec2d(90, 90))));

  auto results = tree.QueryRegion(Range2d(Vec2d(0, 0), Vec2d(50, 50)));
  EXPECT_EQ(2u, results.size());
}

TEST(QuadtreeTest, RemoveItem) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50)));
  tree.Insert(item);

  EXPECT_TRUE(tree.Remove(item));
  auto results = tree.QueryPoint(Vec2d(30, 30));
  EXPECT_TRUE(results.empty());
}

TEST(QuadtreeTest, RemoveNonexistent) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50)));
  EXPECT_FALSE(tree.Remove(item));
}

TEST(QuadtreeTest, RemoveFromEmptyTree) {
  Quadtree tree;
  SpatialItem item(SpatialItemType::Node, "n1", Range2d(Vec2d(0, 0), Vec2d(10, 10)));
  EXPECT_FALSE(tree.Remove(item));
}

TEST(QuadtreeTest, Clear) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));
  tree.Clear();
  EXPECT_FALSE(tree.HasRoot());
}

TEST(QuadtreeTest, Rebuild) {
  Quadtree tree;
  std::vector<SpatialItem> items = {
      SpatialItem(SpatialItemType::Node, "a", Range2d(Vec2d(0, 0), Vec2d(10, 10))),
      SpatialItem(SpatialItemType::Node, "b", Range2d(Vec2d(50, 50), Vec2d(60, 60))),
      SpatialItem(SpatialItemType::Node, "c", Range2d(Vec2d(90, 90), Vec2d(100, 100))),
  };
  tree.Rebuild(items);
  EXPECT_TRUE(tree.HasRoot());

  auto results = tree.QueryPoint(Vec2d(55, 55));
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ("b", results[0].itemId);
}

TEST(QuadtreeTest, RebuildEmpty) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(50, 50))));

  tree.Rebuild({});
  EXPECT_FALSE(tree.HasRoot());
}

TEST(QuadtreeTest, BoundsExpansion) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20))));

  // Insert item outside original bounds
  tree.Insert(SpatialItem(SpatialItemType::Node, "far", Range2d(Vec2d(500, 500), Vec2d(510, 510))));

  // Both items should be queryable
  auto r1 = tree.QueryPoint(Vec2d(15, 15));
  EXPECT_EQ(1u, r1.size());
  EXPECT_EQ("n1", r1[0].itemId);

  auto r2 = tree.QueryPoint(Vec2d(505, 505));
  EXPECT_EQ(1u, r2.size());
  EXPECT_EQ("far", r2[0].itemId);
}

TEST(QuadtreeTest, QueryEmptyTree) {
  Quadtree tree;
  auto results = tree.QueryPoint(Vec2d(0, 0));
  EXPECT_TRUE(results.empty());

  results = tree.QueryRegion(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  EXPECT_TRUE(results.empty());
}

TEST(QuadtreeTest, GetCells) {
  Quadtree tree(Range2d(Vec2d(0, 0), Vec2d(100, 100)));
  tree.Insert(SpatialItem(SpatialItemType::Node, "n1", Range2d(Vec2d(10, 10), Vec2d(20, 20))));

  auto cells = tree.GetCells();
  EXPECT_GE(cells.size(), 1u);
}

// ---------------------------------------------------------------------------
// SpatialIndex
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, DefaultConstruction) {
  SpatialIndex index;
  EXPECT_EQ(0u, index.GetNodeCount());
  EXPECT_EQ(0u, index.GetLinkCount());
}

TEST(SpatialIndexTest, InsertAndQueryNode) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("node_a", Range2d(Vec2d(10, 10), Vec2d(100, 100)));

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(50, 50), nodeIds, linkIndices);

  EXPECT_EQ(1u, nodeIds.size());
  EXPECT_EQ("node_a", nodeIds[0]);
  EXPECT_TRUE(linkIndices.empty());
}

TEST(SpatialIndexTest, InsertAndQueryLink) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertLink(0, Range2d(Vec2d(10, 10), Vec2d(100, 100)));

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(50, 50), nodeIds, linkIndices);

  EXPECT_TRUE(nodeIds.empty());
  EXPECT_EQ(1u, linkIndices.size());
  EXPECT_EQ(0, linkIndices[0]);
}

TEST(SpatialIndexTest, RemoveNode) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("node_a", Range2d(Vec2d(10, 10), Vec2d(100, 100)));
  EXPECT_EQ(1u, index.GetNodeCount());

  EXPECT_TRUE(index.RemoveNode("node_a"));
  EXPECT_EQ(0u, index.GetNodeCount());

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(50, 50), nodeIds, linkIndices);
  EXPECT_TRUE(nodeIds.empty());
}

TEST(SpatialIndexTest, RemoveLink) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertLink(5, Range2d(Vec2d(10, 10), Vec2d(100, 100)));
  EXPECT_EQ(1u, index.GetLinkCount());

  EXPECT_TRUE(index.RemoveLink(5));
  EXPECT_EQ(0u, index.GetLinkCount());
}

TEST(SpatialIndexTest, RemoveNonexistent) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  EXPECT_FALSE(index.RemoveNode("missing"));
  EXPECT_FALSE(index.RemoveLink(99));
}

TEST(SpatialIndexTest, UpdateNode) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("node_a", Range2d(Vec2d(10, 10), Vec2d(100, 100)));

  // Move node to a new position
  index.UpdateNode("node_a", Range2d(Vec2d(500, 500), Vec2d(600, 600)));

  // Old position should miss
  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(50, 50), nodeIds, linkIndices);
  EXPECT_TRUE(nodeIds.empty());

  // New position should hit
  nodeIds.clear();
  index.QueryPoint(Vec2d(550, 550), nodeIds, linkIndices);
  EXPECT_EQ(1u, nodeIds.size());
  EXPECT_EQ("node_a", nodeIds[0]);
}

TEST(SpatialIndexTest, QueryRegion) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("a", Range2d(Vec2d(10, 10), Vec2d(50, 50)));
  index.InsertNode("b", Range2d(Vec2d(100, 100), Vec2d(150, 150)));
  index.InsertNode("c", Range2d(Vec2d(500, 500), Vec2d(600, 600)));
  index.InsertLink(0, Range2d(Vec2d(30, 30), Vec2d(120, 120)));

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryRegion(Range2d(Vec2d(0, 0), Vec2d(200, 200)), nodeIds, linkIndices);

  EXPECT_EQ(2u, nodeIds.size());
  EXPECT_EQ(1u, linkIndices.size());
}

TEST(SpatialIndexTest, BulkInsert) {
  SpatialIndex index;
  std::vector<std::string> nodeIds = {"a", "b", "c"};
  std::vector<Range2d> nodeBounds = {
      Range2d(Vec2d(0, 0), Vec2d(50, 50)),
      Range2d(Vec2d(100, 100), Vec2d(150, 150)),
      Range2d(Vec2d(200, 200), Vec2d(250, 250)),
  };
  std::vector<Range2d> linkBounds = {
      Range2d(Vec2d(25, 25), Vec2d(125, 125)),
  };

  index.BulkInsert(nodeIds, nodeBounds, linkBounds);

  EXPECT_EQ(3u, index.GetNodeCount());
  EXPECT_EQ(1u, index.GetLinkCount());

  std::vector<std::string> resultNodes;
  std::vector<int> resultLinks;
  index.QueryPoint(Vec2d(110, 110), resultNodes, resultLinks);

  EXPECT_EQ(1u, resultNodes.size());
  EXPECT_EQ("b", resultNodes[0]);
  EXPECT_EQ(1u, resultLinks.size());
  EXPECT_EQ(0, resultLinks[0]);
}

TEST(SpatialIndexTest, BulkInsertClearsPrevious) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("old", Range2d(Vec2d(10, 10), Vec2d(50, 50)));

  index.BulkInsert({"new"}, {Range2d(Vec2d(100, 100), Vec2d(150, 150))}, {});

  EXPECT_EQ(1u, index.GetNodeCount());

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(30, 30), nodeIds, linkIndices);
  EXPECT_TRUE(nodeIds.empty());
}

TEST(SpatialIndexTest, Clear) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("a", Range2d(Vec2d(10, 10), Vec2d(50, 50)));
  index.InsertLink(0, Range2d(Vec2d(60, 60), Vec2d(100, 100)));

  index.Clear();
  EXPECT_EQ(0u, index.GetNodeCount());
  EXPECT_EQ(0u, index.GetLinkCount());
}

TEST(SpatialIndexTest, InsertNodeReplacesExisting) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("a", Range2d(Vec2d(10, 10), Vec2d(50, 50)));
  index.InsertNode("a", Range2d(Vec2d(500, 500), Vec2d(600, 600)));

  EXPECT_EQ(1u, index.GetNodeCount());

  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  // Old position should miss
  index.QueryPoint(Vec2d(30, 30), nodeIds, linkIndices);
  EXPECT_TRUE(nodeIds.empty());

  // New position should hit
  index.QueryPoint(Vec2d(550, 550), nodeIds, linkIndices);
  EXPECT_EQ(1u, nodeIds.size());
}

TEST(SpatialIndexTest, GetCells) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(1000, 1000)));
  index.InsertNode("a", Range2d(Vec2d(10, 10), Vec2d(50, 50)));

  auto cells = index.GetCells();
  EXPECT_GE(cells.size(), 1u);
}

// ---------------------------------------------------------------------------
// SpatialIndex -- ComputeNodeBounds / ComputeLinkBounds
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, ComputeNodeBounds) {
  NodeData node;
  node.position = Vec2d(100, 200);
  node.size = Vec2d(300, 150);

  auto bounds = SpatialIndex::ComputeNodeBounds(node);
  EXPECT_EQ(Vec2d(100, 200), bounds.GetMin());
  EXPECT_EQ(Vec2d(400, 350), bounds.GetMax());
}

TEST(SpatialIndexTest, ComputeLinkBoundsHorizontal) {
  LinkData link;
  link.start = Vec2d(0, 50);
  link.end = Vec2d(100, 50);

  auto bounds = SpatialIndex::ComputeLinkBounds(link);

  // dx=100, dy=0, margin = max(100,0)*0.3 = 30
  EXPECT_DOUBLE_EQ(-30.0, bounds.GetMin()[0]);
  EXPECT_DOUBLE_EQ(20.0, bounds.GetMin()[1]);
  EXPECT_DOUBLE_EQ(130.0, bounds.GetMax()[0]);
  EXPECT_DOUBLE_EQ(80.0, bounds.GetMax()[1]);
}

TEST(SpatialIndexTest, ComputeLinkBoundsVertical) {
  LinkData link;
  link.start = Vec2d(50, 0);
  link.end = Vec2d(50, 200);

  auto bounds = SpatialIndex::ComputeLinkBounds(link);

  // dx=0, dy=200, margin = max(0,200)*0.3 = 60
  EXPECT_DOUBLE_EQ(-10.0, bounds.GetMin()[0]);
  EXPECT_DOUBLE_EQ(-60.0, bounds.GetMin()[1]);
  EXPECT_DOUBLE_EQ(110.0, bounds.GetMax()[0]);
  EXPECT_DOUBLE_EQ(260.0, bounds.GetMax()[1]);
}

// ---------------------------------------------------------------------------
// SpatialIndex -- many items stress test
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, ManyNodesQueryCorrectness) {
  SpatialIndex index(Range2d(Vec2d(0, 0), Vec2d(10000, 10000)));

  // Insert a grid of nodes
  for (int i = 0; i < 100; ++i) {
    double x = (i % 10) * 100.0;
    double y = (i / 10) * 100.0; // NOLINT(bugprone-integer-division)
    index.InsertNode("n" + std::to_string(i), Range2d(Vec2d(x, y), Vec2d(x + 50, y + 50)));
  }
  EXPECT_EQ(100u, index.GetNodeCount());

  // Query a specific node
  std::vector<std::string> nodeIds;
  std::vector<int> linkIndices;
  index.QueryPoint(Vec2d(25, 25), nodeIds, linkIndices);
  EXPECT_EQ(1u, nodeIds.size());
  EXPECT_EQ("n0", nodeIds[0]);

  // Query a region that should contain several nodes
  nodeIds.clear();
  index.QueryRegion(Range2d(Vec2d(0, 0), Vec2d(250, 150)), nodeIds, linkIndices);
  EXPECT_GE(nodeIds.size(), 3u);
}

} // namespace noodles
