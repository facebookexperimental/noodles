// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

//
// Headless integration tests for the noodles C++ core.
//
// Tests the complete create → move → undo → redo lifecycle using
// GraphModel + NoodlesUndoManager, without any Qt or Python dependency.
// These run in CI on all platforms via the existing noodles_test target
// (which uses OSMesa for software GL on Linux).

#include <gtest/gtest.h>

#include "core/NodeData.h"
#include "core/RenderConfig.h"
#include "spatial/SpatialIndex.h"
#include "undo/CompoundCommand.h"
#include "undo/LambdaCommand.h"
#include "undo/NoodlesUndoManager.h"

#include <string>

namespace noodles {

// ---------------------------------------------------------------------------
// Test fixture: GraphModel + UndoManager lifecycle
// ---------------------------------------------------------------------------

class NoodlesHeadlessTest : public ::testing::Test {
 protected:
  GraphModel graph;

  void SetUp() override {
    NoodlesUndoManager::instance().clear();
    graph.clear();
  }

  void TearDown() override {
    NoodlesUndoManager::instance().clear();
    graph.clear();
  }

  /// Create a node in the graph with the given id, type, and position.
  NodeData& createNode(
      const std::string& id,
      const std::string& type,
      Vec2d position,
      const std::vector<std::string>& inputs = {},
      const std::vector<std::string>& outputs = {}) {
    NodeData node;
    node.id = id;
    node.name = id;
    node.type = type;
    node.position = position;
    node.size = {200.0, 100.0};
    node.inputPins = inputs;
    node.outputPins = outputs;
    graph.nodes[id] = std::move(node);
    return graph.nodes[id];
  }

  /// Move a node and push an undo command.
  void moveNodeWithUndo(const std::string& nodeId, Vec2d newPos) {
    auto it = graph.nodes.find(nodeId);
    ASSERT_NE(it, graph.nodes.end()) << "Node not found: " << nodeId;

    Vec2d oldPos = it->second.position;
    it->second.position = newPos;

    auto& mgr = NoodlesUndoManager::instance();
    // Capture by value for undo/redo lambdas
    auto* graphPtr = &graph;
    mgr.pushCommand(
        std::make_unique<LambdaCommand>(
            "Move " + nodeId,
            [graphPtr, nodeId, newPos]() {
              auto it = graphPtr->nodes.find(nodeId);
              if (it != graphPtr->nodes.end()) {
                it->second.position = newPos;
              }
            },
            [graphPtr, nodeId, oldPos]() {
              auto it = graphPtr->nodes.find(nodeId);
              if (it != graphPtr->nodes.end()) {
                it->second.position = oldPos;
              }
            }));
  }

  /// Delete a node and push an undo command.
  void deleteNodeWithUndo(const std::string& nodeId) {
    auto it = graph.nodes.find(nodeId);
    ASSERT_NE(it, graph.nodes.end()) << "Node not found: " << nodeId;

    NodeData savedNode = it->second;
    graph.nodes.erase(it);

    auto& mgr = NoodlesUndoManager::instance();
    auto* graphPtr = &graph;
    mgr.pushCommand(
        std::make_unique<LambdaCommand>(
            "Delete " + nodeId,
            [graphPtr, nodeId]() { graphPtr->nodes.erase(nodeId); },
            [graphPtr, nodeId, savedNode]() { graphPtr->nodes[nodeId] = savedNode; }));
  }

  /// Add a link between two nodes and push an undo command.
  void addLinkWithUndo(
      const std::string& srcId,
      const std::string& srcPort,
      const std::string& tgtId,
      const std::string& tgtPort) {
    LinkData link;
    link.sourceNodeId = srcId;
    link.sourcePort = srcPort;
    link.targetNodeId = tgtId;
    link.targetPort = tgtPort;
    graph.links.push_back(link);
    graph.markLinksChanged();

    auto& mgr = NoodlesUndoManager::instance();
    auto* graphPtr = &graph;
    mgr.pushCommand(
        std::make_unique<LambdaCommand>(
            "Connect " + srcId + " → " + tgtId,
            [graphPtr, link]() {
              graphPtr->links.push_back(link);
              graphPtr->markLinksChanged();
            },
            [graphPtr]() {
              if (!graphPtr->links.empty()) {
                graphPtr->links.pop_back();
                graphPtr->markLinksChanged();
              }
            }));
  }
};

// ---------------------------------------------------------------------------
// Node creation
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, CreateSingleNode) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  EXPECT_EQ(1u, graph.nodes.size());
  EXPECT_EQ(100.0, graph.nodes["/BP/NodeA"].position[0]);
  EXPECT_EQ(200.0, graph.nodes["/BP/NodeA"].position[1]);
}

TEST_F(NoodlesHeadlessTest, CreateMultipleNodes) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0});
  createNode("/BP/NodeB", "TypeB", {300.0, 100.0});
  createNode("/BP/NodeC", "TypeC", {200.0, 300.0});
  EXPECT_EQ(3u, graph.nodes.size());
}

TEST_F(NoodlesHeadlessTest, CreateNodeWithPins) {
  auto& node = createNode("/BP/Add0", "AddFloatNode", {100.0, 100.0}, {"x", "y"}, {"result"});
  EXPECT_EQ(2u, node.inputPins.size());
  EXPECT_EQ(1u, node.outputPins.size());
  EXPECT_EQ("x", node.inputPins[0]);
  EXPECT_EQ("result", node.outputPins[0]);
}

// ---------------------------------------------------------------------------
// Node movement
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, MoveNodeChangesPosition) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});
  EXPECT_EQ(300.0, graph.nodes["/BP/NodeA"].position[0]);
  EXPECT_EQ(400.0, graph.nodes["/BP/NodeA"].position[1]);
}

TEST_F(NoodlesHeadlessTest, MoveNodePushesUndo) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  EXPECT_FALSE(NoodlesUndoManager::instance().canUndo());
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});
  EXPECT_TRUE(NoodlesUndoManager::instance().canUndo());
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, UndoMoveRevertsPosition) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});

  NoodlesUndoManager::instance().undo();

  EXPECT_EQ(100.0, graph.nodes["/BP/NodeA"].position[0]);
  EXPECT_EQ(200.0, graph.nodes["/BP/NodeA"].position[1]);
}

TEST_F(NoodlesHeadlessTest, UndoEnablesRedo) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});
  NoodlesUndoManager::instance().undo();
  EXPECT_TRUE(NoodlesUndoManager::instance().canRedo());
}

TEST_F(NoodlesHeadlessTest, RedoReappliesMove) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});

  NoodlesUndoManager::instance().undo();
  NoodlesUndoManager::instance().redo();

  EXPECT_EQ(300.0, graph.nodes["/BP/NodeA"].position[0]);
  EXPECT_EQ(400.0, graph.nodes["/BP/NodeA"].position[1]);
}

TEST_F(NoodlesHeadlessTest, MultipleUndoRedoCycles) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  moveNodeWithUndo("/BP/NodeA", {300.0, 400.0});
  moveNodeWithUndo("/BP/NodeA", {500.0, 600.0});

  auto& mgr = NoodlesUndoManager::instance();

  // Undo second move
  mgr.undo();
  EXPECT_EQ(300.0, graph.nodes["/BP/NodeA"].position[0]);

  // Undo first move
  mgr.undo();
  EXPECT_EQ(100.0, graph.nodes["/BP/NodeA"].position[0]);

  // Redo first move
  mgr.redo();
  EXPECT_EQ(300.0, graph.nodes["/BP/NodeA"].position[0]);

  // Redo second move
  mgr.redo();
  EXPECT_EQ(500.0, graph.nodes["/BP/NodeA"].position[0]);
}

// ---------------------------------------------------------------------------
// Delete with undo
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, DeleteNodeRemovesIt) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0});
  deleteNodeWithUndo("/BP/NodeA");
  EXPECT_EQ(0u, graph.nodes.size());
}

TEST_F(NoodlesHeadlessTest, UndoDeleteRestoresNode) {
  createNode("/BP/NodeA", "TestType", {100.0, 200.0}, {"x"}, {"result"});
  deleteNodeWithUndo("/BP/NodeA");

  NoodlesUndoManager::instance().undo();

  EXPECT_EQ(1u, graph.nodes.size());
  auto& restored = graph.nodes["/BP/NodeA"];
  EXPECT_EQ("TestType", restored.type);
  EXPECT_EQ(100.0, restored.position[0]);
  EXPECT_EQ(1u, restored.inputPins.size());
  EXPECT_EQ(1u, restored.outputPins.size());
}

// ---------------------------------------------------------------------------
// Connections
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, AddLinkConnectsNodes) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0}, {}, {"out"});
  createNode("/BP/NodeB", "TypeB", {300.0, 100.0}, {"in"}, {});
  addLinkWithUndo("/BP/NodeA", "out", "/BP/NodeB", "in");

  EXPECT_EQ(1u, graph.links.size());
  EXPECT_EQ("/BP/NodeA", graph.links[0].sourceNodeId);
  EXPECT_EQ("/BP/NodeB", graph.links[0].targetNodeId);
}

TEST_F(NoodlesHeadlessTest, UndoLinkRemovesConnection) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0}, {}, {"out"});
  createNode("/BP/NodeB", "TypeB", {300.0, 100.0}, {"in"}, {});
  addLinkWithUndo("/BP/NodeA", "out", "/BP/NodeB", "in");

  NoodlesUndoManager::instance().undo();
  EXPECT_EQ(0u, graph.links.size());
}

// ---------------------------------------------------------------------------
// Multi-node compound operations
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, BatchDeleteWithCompoundUndo) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0});
  createNode("/BP/NodeB", "TypeB", {300.0, 100.0});
  createNode("/BP/NodeC", "TypeC", {200.0, 300.0});

  // Batch delete A and C using a compound command
  NodeData savedA = graph.nodes["/BP/NodeA"];
  NodeData savedC = graph.nodes["/BP/NodeC"];
  graph.nodes.erase("/BP/NodeA");
  graph.nodes.erase("/BP/NodeC");

  auto compound = std::make_unique<CompoundCommand>("Delete Selected");
  auto* graphPtr = &graph;
  compound->addCommand(
      std::make_unique<LambdaCommand>(
          "Delete A",
          [graphPtr]() { graphPtr->nodes.erase("/BP/NodeA"); },
          [graphPtr, savedA]() { graphPtr->nodes["/BP/NodeA"] = savedA; }));
  compound->addCommand(
      std::make_unique<LambdaCommand>(
          "Delete C",
          [graphPtr]() { graphPtr->nodes.erase("/BP/NodeC"); },
          [graphPtr, savedC]() { graphPtr->nodes["/BP/NodeC"] = savedC; }));
  NoodlesUndoManager::instance().pushCommand(std::move(compound));

  EXPECT_EQ(1u, graph.nodes.size());
  EXPECT_TRUE(graph.nodes.count("/BP/NodeB"));

  // Single undo restores both
  NoodlesUndoManager::instance().undo();
  EXPECT_EQ(3u, graph.nodes.size());
  EXPECT_EQ(100.0, graph.nodes["/BP/NodeA"].position[0]);
  EXPECT_EQ(200.0, graph.nodes["/BP/NodeC"].position[0]);
}

// ---------------------------------------------------------------------------
// Spatial index integration
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, SpatialIndexTracksNodes) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0});
  graph.nodes["/BP/NodeA"].size = {200.0, 100.0};

  Range2d worldBounds{{0.0, 0.0}, {1000.0, 1000.0}};
  SpatialIndex index(worldBounds);
  for (const auto& [id, node] : graph.nodes) {
    Range2d bounds{
        node.position, {node.position[0] + node.size[0], node.position[1] + node.size[1]}};
    index.InsertNode(id, bounds);
  }

  // Query a point inside the node
  std::vector<std::string> nodeIds;
  std::vector<int> linkIds;
  index.QueryPoint({150.0, 150.0}, nodeIds, linkIds);
  EXPECT_EQ(1u, nodeIds.size());
  EXPECT_EQ("/BP/NodeA", nodeIds[0]);

  // Query a point outside all nodes
  nodeIds.clear();
  linkIds.clear();
  index.QueryPoint({500.0, 500.0}, nodeIds, linkIds);
  EXPECT_EQ(0u, nodeIds.size());
}

TEST_F(NoodlesHeadlessTest, SpatialIndexUpdatesAfterMove) {
  createNode("/BP/NodeA", "TypeA", {100.0, 100.0});
  graph.nodes["/BP/NodeA"].size = {200.0, 100.0};

  Range2d worldBounds{{0.0, 0.0}, {1000.0, 1000.0}};
  SpatialIndex index(worldBounds);
  Range2d bounds{graph.nodes["/BP/NodeA"].position, {300.0, 200.0}};
  index.InsertNode("/BP/NodeA", bounds);

  // Move node
  graph.nodes["/BP/NodeA"].position = {500.0, 500.0};
  Range2d newBounds{{500.0, 500.0}, {700.0, 600.0}};
  index.UpdateNode("/BP/NodeA", newBounds);

  // Old position should miss
  std::vector<std::string> nodeIds;
  std::vector<int> linkIds;
  index.QueryPoint({150.0, 150.0}, nodeIds, linkIds);
  EXPECT_EQ(0u, nodeIds.size());

  // New position should hit
  nodeIds.clear();
  linkIds.clear();
  index.QueryPoint({550.0, 550.0}, nodeIds, linkIds);
  EXPECT_EQ(1u, nodeIds.size());
}

// ---------------------------------------------------------------------------
// Node sizing
// ---------------------------------------------------------------------------

TEST_F(NoodlesHeadlessTest, CalculateNodeSizeWithPins) {
  auto& node = createNode("/BP/Add0", "AddFloatNode", {0.0, 0.0}, {"x", "y"}, {"result"});

  FontMetrics fm;
  fm.ascender = 0.8;
  fm.descender = -0.2;
  fm.lineHeight = 1.2;

  // Simple text width callback
  auto textWidth = [](const std::string& text, double fontSize) -> double {
    return text.length() * fontSize * 0.5;
  };

  RenderConfig config;
  graph.calculateNodeSize(node, textWidth, fm, &config);

  EXPECT_GT(node.size[0], 0.0);
  EXPECT_GT(node.size[1], 0.0);
  // Node with 2 input pins should be taller than minimum
  EXPECT_GT(node.size[1], 100.0);
}

} // namespace noodles
