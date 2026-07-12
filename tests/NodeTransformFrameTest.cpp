// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

//
// Unit tests for NodeTransformFrame's slot bookkeeping (stable slots, prune +
// reuse), base positions, and offset accumulation. GL-free (no upload/bind).

#include <gtest/gtest.h>

#include "core/NodeData.h"
#include "render/NodeTransformFrame.h"

#include <set>
#include <string>
#include <unordered_map>

namespace noodles {
namespace {

NodeData nodeAt(const std::string& id, double x, double y) {
  NodeData node;
  node.id = id;
  node.name = id;
  node.position = {x, y};
  return node;
}

std::unordered_map<std::string, NodeData> graphOf(
    std::initializer_list<std::pair<const std::string, NodeData>> items) {
  return std::unordered_map<std::string, NodeData>(items);
}

} // namespace

TEST(NodeTransformFrameTest, AssignsDistinctSlotsAndBasePositions) {
  NodeTransformFrame frame;
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 10.0, 20.0)}, {"b", nodeAt("b", 30.0, 40.0)}}));

  EXPECT_GE(frame.shaderIndex("a"), 0.0f);
  EXPECT_GE(frame.shaderIndex("b"), 0.0f);
  EXPECT_NE(frame.shaderIndex("a"), frame.shaderIndex("b"));

  const Vec2d basePosA = frame.basePosition("a");
  EXPECT_DOUBLE_EQ(10.0, basePosA[0]);
  EXPECT_DOUBLE_EQ(20.0, basePosA[1]);
}

TEST(NodeTransformFrameTest, UnknownNodeHasNoSlotOrTransform) {
  NodeTransformFrame frame;
  EXPECT_FLOAT_EQ(-1.0f, frame.shaderIndex("ghost"));
  const Vec2d basePos = frame.basePosition("ghost");
  EXPECT_DOUBLE_EQ(0.0, basePos[0]);
  EXPECT_DOUBLE_EQ(0.0, basePos[1]);
}

TEST(NodeTransformFrameTest, SlotsStableAcrossResyncAndAddsAppend) {
  NodeTransformFrame frame;
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 0.0, 0.0)}, {"b", nodeAt("b", 0.0, 0.0)}}));
  const float slotA = frame.shaderIndex("a");
  const float slotB = frame.shaderIndex("b");

  // Re-sync the same set (the live map rehashes every change) — slots unchanged.
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 0.0, 0.0)}, {"b", nodeAt("b", 0.0, 0.0)}}));
  EXPECT_FLOAT_EQ(slotA, frame.shaderIndex("a"));
  EXPECT_FLOAT_EQ(slotB, frame.shaderIndex("b"));

  // Add a node — existing slots keep, new one is distinct.
  frame.syncFromNodes(graphOf(
      {{"a", nodeAt("a", 0.0, 0.0)}, {"b", nodeAt("b", 0.0, 0.0)}, {"c", nodeAt("c", 0.0, 0.0)}}));
  EXPECT_FLOAT_EQ(slotA, frame.shaderIndex("a"));
  EXPECT_FLOAT_EQ(slotB, frame.shaderIndex("b"));
  EXPECT_NE(frame.shaderIndex("c"), slotA);
  EXPECT_NE(frame.shaderIndex("c"), slotB);
}

TEST(NodeTransformFrameTest, PrunedSlotIsReused) {
  NodeTransformFrame frame;
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 0.0, 0.0)}, {"b", nodeAt("b", 0.0, 0.0)}}));
  const float slotB = frame.shaderIndex("b");

  // Delete b: its slot is freed.
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 0.0, 0.0)}}));
  EXPECT_FLOAT_EQ(-1.0f, frame.shaderIndex("b"));

  // A new node reuses the freed slot rather than growing the table.
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 0.0, 0.0)}, {"c", nodeAt("c", 0.0, 0.0)}}));
  EXPECT_FLOAT_EQ(slotB, frame.shaderIndex("c"));
}

TEST(NodeTransformFrameTest, TranslateAccumulatesOffset) {
  NodeTransformFrame frame;
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 5.0, 5.0)}}));

  // New node starts at zero offset (geometry is baked at base).
  EXPECT_FLOAT_EQ(0.0f, frame.offsetForTest("a").first);
  EXPECT_FLOAT_EQ(0.0f, frame.offsetForTest("a").second);

  frame.translate("a", 3.0, -2.0);
  frame.translate("a", 1.0, 4.0);
  EXPECT_FLOAT_EQ(4.0f, frame.offsetForTest("a").first);
  EXPECT_FLOAT_EQ(2.0f, frame.offsetForTest("a").second);

  // A moved node keeps its accumulated offset across a content-triggered resync
  // (base/offset persist; only the geometry re-bakes at base).
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 99.0, 99.0)}}));
  EXPECT_FLOAT_EQ(4.0f, frame.offsetForTest("a").first);
  EXPECT_FLOAT_EQ(2.0f, frame.offsetForTest("a").second);
}

TEST(NodeTransformFrameTest, ResetClearsEverything) {
  NodeTransformFrame frame;
  frame.syncFromNodes(graphOf({{"a", nodeAt("a", 1.0, 1.0)}}));
  frame.translate("a", 7.0, 7.0);

  frame.reset();
  EXPECT_FLOAT_EQ(-1.0f, frame.shaderIndex("a"));
  EXPECT_TRUE(frame.idToIndexForTest().empty());
}

TEST(NodeTransformFrameTest, GrowsToFitBeyondInitialCapacity) {
  // Tiny seed forces the buffer to grow past its initial capacity.
  NodeTransformFrame frame(2);
  std::unordered_map<std::string, NodeData> nodes;
  for (int i = 0; i < 5; ++i) {
    const std::string id = "n" + std::to_string(i);
    nodes.emplace(id, nodeAt(id, i, i));
  }
  frame.syncFromNodes(nodes);

  // Every node should have a valid, distinct slot.
  EXPECT_GE(frame.capacity(), 5);
  std::set<int> slots;
  for (int i = 0; i < 5; ++i) {
    const float idx = frame.shaderIndex("n" + std::to_string(i));
    EXPECT_GE(idx, 0.0f);
    slots.insert(static_cast<int>(idx));
  }
  EXPECT_EQ(5u, slots.size());
}

TEST(NodeTransformFrameTest, CapacityDoesNotShrinkOnPrune) {
  NodeTransformFrame frame(2);
  std::unordered_map<std::string, NodeData> nodes;
  for (int i = 0; i < 8; ++i) {
    const std::string id = "n" + std::to_string(i);
    nodes.emplace(id, nodeAt(id, 0.0, 0.0));
  }
  frame.syncFromNodes(nodes);
  const int grown = frame.capacity();
  EXPECT_GE(grown, 8);

  // Shrinking would invalidate stable slots.
  frame.syncFromNodes(graphOf({{"n0", nodeAt("n0", 0.0, 0.0)}}));
  EXPECT_EQ(grown, frame.capacity());
  EXPECT_GE(frame.shaderIndex("n0"), 0.0f);
}

TEST(NodeTransformFrameTest, GrowthFollowsDoublingPolicy) {
  // Pin the doubling sequence: seed=2, after one growth slot >=3 must give
  // capacity 4, after another (slot >=5) must give 8, etc. A future change
  // that, say, switched to +1 increments would balloon the upload cost; a
  // change to doubling-from-larger-base would reintroduce the >2048 silent
  // drop this diff was meant to fix.
  NodeTransformFrame frame(2);
  EXPECT_EQ(2, frame.capacity());

  std::unordered_map<std::string, NodeData> nodes;
  nodes.emplace("n0", nodeAt("n0", 0.0, 0.0));
  nodes.emplace("n1", nodeAt("n1", 0.0, 0.0));
  frame.syncFromNodes(nodes);
  // Exactly fits: capacity stays at the seed (boundary case).
  EXPECT_EQ(2, frame.capacity());

  // 3rd node forces one doubling (2 -> 4).
  nodes.emplace("n2", nodeAt("n2", 0.0, 0.0));
  frame.syncFromNodes(nodes);
  EXPECT_EQ(4, frame.capacity());

  // 5th node forces another doubling (4 -> 8).
  nodes.emplace("n3", nodeAt("n3", 0.0, 0.0));
  nodes.emplace("n4", nodeAt("n4", 0.0, 0.0));
  frame.syncFromNodes(nodes);
  EXPECT_EQ(8, frame.capacity());

  // Already-fits resync: capacity does not change (no spurious growth).
  frame.syncFromNodes(nodes);
  EXPECT_EQ(8, frame.capacity());
}

} // namespace noodles
