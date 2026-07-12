// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "undo/CompoundCommand.h"
#include "undo/LambdaCommand.h"
#include "undo/NoodlesUndoManager.h"

namespace noodles {

class NoodlesUndoManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    resetUndoManager();
  }

  void TearDown() override {
    resetUndoManager();
  }

  void resetUndoManager() {
    auto& mgr = NoodlesUndoManager::instance();
    mgr.setMaxStackDepth(100);
    mgr.clear();
  }

  /// Helper to push a simple no-op command with a description.
  void pushNoOp(const std::string& desc) {
    NoodlesUndoManager::instance().pushCommand(
        std::make_unique<LambdaCommand>(desc, []() {}, []() {}));
  }
};

// --- Basic stack tests ---

TEST_F(NoodlesUndoManagerTest, InitiallyEmpty) {
  auto& mgr = NoodlesUndoManager::instance();
  EXPECT_FALSE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
  EXPECT_TRUE(mgr.undoDescription().empty());
  EXPECT_TRUE(mgr.redoDescription().empty());
}

TEST_F(NoodlesUndoManagerTest, PushCommandEnablesUndo) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Test Action");
  EXPECT_TRUE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
  EXPECT_EQ("Test Action", mgr.undoDescription());
}

TEST_F(NoodlesUndoManagerTest, UndoMovesToRedoStack) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Test Action");
  mgr.undo();
  EXPECT_FALSE(mgr.canUndo());
  EXPECT_TRUE(mgr.canRedo());
  EXPECT_EQ("Test Action", mgr.redoDescription());
}

TEST_F(NoodlesUndoManagerTest, RedoMovesBackToUndoStack) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Test Action");
  mgr.undo();
  mgr.redo();
  EXPECT_TRUE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
  EXPECT_EQ("Test Action", mgr.undoDescription());
}

TEST_F(NoodlesUndoManagerTest, NewEditClearsRedoStack) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Action 1");
  mgr.undo();
  EXPECT_TRUE(mgr.canRedo());
  pushNoOp("Action 2");
  EXPECT_FALSE(mgr.canRedo());
  EXPECT_EQ("Action 2", mgr.undoDescription());
}

TEST_F(NoodlesUndoManagerTest, ClearEmptiesBothStacks) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Action 1");
  pushNoOp("Action 2");
  mgr.undo();
  EXPECT_TRUE(mgr.canUndo());
  EXPECT_TRUE(mgr.canRedo());
  mgr.clear();
  EXPECT_FALSE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
}

TEST_F(NoodlesUndoManagerTest, StackDepthLimiting) {
  auto& mgr = NoodlesUndoManager::instance();
  mgr.setMaxStackDepth(3);
  for (int i = 0; i < 5; ++i) {
    pushNoOp("Action " + std::to_string(i));
  }
  int undoCount = 0;
  while (mgr.canUndo()) {
    mgr.undo();
    ++undoCount;
  }
  EXPECT_EQ(3, undoCount);
}

TEST_F(NoodlesUndoManagerTest, SetMaxStackDepthTrimsExisting) {
  auto& mgr = NoodlesUndoManager::instance();
  for (int i = 0; i < 10; ++i) {
    pushNoOp("Action " + std::to_string(i));
  }
  mgr.setMaxStackDepth(2);
  int undoCount = 0;
  while (mgr.canUndo()) {
    mgr.undo();
    ++undoCount;
  }
  EXPECT_EQ(2, undoCount);
}

TEST_F(NoodlesUndoManagerTest, UndoOnEmptyIsNoOp) {
  auto& mgr = NoodlesUndoManager::instance();
  mgr.undo();
  EXPECT_FALSE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
}

TEST_F(NoodlesUndoManagerTest, RedoOnEmptyIsNoOp) {
  auto& mgr = NoodlesUndoManager::instance();
  mgr.redo();
  EXPECT_FALSE(mgr.canUndo());
  EXPECT_FALSE(mgr.canRedo());
}

TEST_F(NoodlesUndoManagerTest, MultipleUndoRedoCycles) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Action 1");
  pushNoOp("Action 2");
  pushNoOp("Action 3");

  EXPECT_EQ("Action 3", mgr.undoDescription());

  mgr.undo();
  EXPECT_EQ("Action 2", mgr.undoDescription());
  EXPECT_EQ("Action 3", mgr.redoDescription());

  mgr.undo();
  EXPECT_EQ("Action 1", mgr.undoDescription());
  EXPECT_EQ("Action 2", mgr.redoDescription());

  mgr.redo();
  EXPECT_EQ("Action 2", mgr.undoDescription());
  EXPECT_EQ("Action 3", mgr.redoDescription());
}

TEST_F(NoodlesUndoManagerTest, SetMaxStackDepthZeroDiscardsAll) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Will Be Discarded");
  EXPECT_TRUE(mgr.canUndo());
  mgr.setMaxStackDepth(0);
  EXPECT_FALSE(mgr.canUndo());
  pushNoOp("Also Discarded");
  EXPECT_FALSE(mgr.canUndo());
}

TEST_F(NoodlesUndoManagerTest, NullCommandIsIgnored) {
  auto& mgr = NoodlesUndoManager::instance();
  mgr.pushCommand(nullptr);
  EXPECT_FALSE(mgr.canUndo());
}

TEST_F(NoodlesUndoManagerTest, DescriptionPreservedThroughUndoRedo) {
  auto& mgr = NoodlesUndoManager::instance();
  pushNoOp("Persistent Description");
  EXPECT_EQ("Persistent Description", mgr.undoDescription());
  mgr.undo();
  EXPECT_EQ("Persistent Description", mgr.redoDescription());
  mgr.redo();
  EXPECT_EQ("Persistent Description", mgr.undoDescription());
}

// --- LambdaCommand tests ---

TEST_F(NoodlesUndoManagerTest, LambdaCommandExecuteAndUndo) {
  auto& mgr = NoodlesUndoManager::instance();
  int value = 10;

  mgr.pushCommand(
      std::make_unique<LambdaCommand>(
          "Set Value", [&value]() { value = 42; }, [&value]() { value = 10; }));

  // pushCommand doesn't call execute() — the caller already applied the edit
  EXPECT_EQ(10, value);

  // Undo calls cmd->undo()
  mgr.undo();
  EXPECT_EQ(10, value);

  // Redo calls cmd->execute()
  mgr.redo();
  EXPECT_EQ(42, value);

  // Another cycle
  mgr.undo();
  EXPECT_EQ(10, value);
  mgr.redo();
  EXPECT_EQ(42, value);
}

TEST_F(NoodlesUndoManagerTest, LambdaCommandMultipleValues) {
  auto& mgr = NoodlesUndoManager::instance();
  int a = 1, b = 3;

  // Simulate: set a=2, b=4
  a = 2;
  b = 4;
  mgr.pushCommand(
      std::make_unique<LambdaCommand>(
          "Multi Edit",
          [&a, &b]() {
            a = 2;
            b = 4;
          },
          [&a, &b]() {
            a = 1;
            b = 3;
          }));

  EXPECT_EQ(2, a);
  EXPECT_EQ(4, b);

  mgr.undo();
  EXPECT_EQ(1, a);
  EXPECT_EQ(3, b);

  mgr.redo();
  EXPECT_EQ(2, a);
  EXPECT_EQ(4, b);
}

// --- CompoundCommand tests ---

TEST_F(NoodlesUndoManagerTest, CompoundCommandEmpty) {
  auto compound = std::make_unique<CompoundCommand>("Empty Compound");
  EXPECT_TRUE(compound->empty());
  EXPECT_EQ(0u, compound->size());
  // No-op execute/undo on empty compound
  compound->execute();
  compound->undo();
}

TEST_F(NoodlesUndoManagerTest, CompoundCommandExecuteOrder) {
  std::vector<int> order;
  auto compound = std::make_unique<CompoundCommand>("Ordered");

  compound->addCommand(
      std::make_unique<LambdaCommand>("A", [&order]() { order.push_back(1); }, []() {}));
  compound->addCommand(
      std::make_unique<LambdaCommand>("B", [&order]() { order.push_back(2); }, []() {}));
  compound->addCommand(
      std::make_unique<LambdaCommand>("C", [&order]() { order.push_back(3); }, []() {}));

  compound->execute();

  const std::vector<int> expected{1, 2, 3};
  EXPECT_EQ(expected, order);
}

TEST_F(NoodlesUndoManagerTest, CompoundCommandUndoReverseOrder) {
  std::vector<int> order;
  auto compound = std::make_unique<CompoundCommand>("Reversed");

  compound->addCommand(
      std::make_unique<LambdaCommand>("A", []() {}, [&order]() { order.push_back(1); }));
  compound->addCommand(
      std::make_unique<LambdaCommand>("B", []() {}, [&order]() { order.push_back(2); }));
  compound->addCommand(
      std::make_unique<LambdaCommand>("C", []() {}, [&order]() { order.push_back(3); }));

  compound->undo();

  // Undo executes children in reverse: C, B, A
  const std::vector<int> expected{3, 2, 1};
  EXPECT_EQ(expected, order);
}

TEST_F(NoodlesUndoManagerTest, CompoundCommandWithManager) {
  auto& mgr = NoodlesUndoManager::instance();

  bool nodeA = true, nodeB = true, nodeC = true;

  // Simulate batch delete of 3 nodes
  nodeA = false;
  nodeB = false;
  nodeC = false;

  auto compound = std::make_unique<CompoundCommand>("Delete Selected");
  compound->addCommand(
      std::make_unique<LambdaCommand>(
          "Delete A", [&nodeA]() { nodeA = false; }, [&nodeA]() { nodeA = true; }));
  compound->addCommand(
      std::make_unique<LambdaCommand>(
          "Delete B", [&nodeB]() { nodeB = false; }, [&nodeB]() { nodeB = true; }));
  compound->addCommand(
      std::make_unique<LambdaCommand>(
          "Delete C", [&nodeC]() { nodeC = false; }, [&nodeC]() { nodeC = true; }));

  mgr.pushCommand(std::move(compound));

  EXPECT_FALSE(nodeA);
  EXPECT_FALSE(nodeB);
  EXPECT_FALSE(nodeC);

  // Single undo restores all three
  mgr.undo();
  EXPECT_TRUE(nodeA);
  EXPECT_TRUE(nodeB);
  EXPECT_TRUE(nodeC);

  // Redo re-deletes all three
  mgr.redo();
  EXPECT_FALSE(nodeA);
  EXPECT_FALSE(nodeB);
  EXPECT_FALSE(nodeC);
}

TEST_F(NoodlesUndoManagerTest, CompoundCommandDescription) {
  auto& mgr = NoodlesUndoManager::instance();
  auto compound = std::make_unique<CompoundCommand>("My Compound");
  EXPECT_EQ("My Compound", compound->description());

  mgr.pushCommand(std::move(compound));
  EXPECT_EQ("My Compound", mgr.undoDescription());
}

// --- State preservation tests ---

TEST_F(NoodlesUndoManagerTest, StatePreservedAcrossUndoRedoCycles) {
  auto& mgr = NoodlesUndoManager::instance();

  struct NodeState {
    bool active;
    int zOrder;
  };

  NodeState front{true, 5};
  NodeState back{true, 2};

  // Delete the front node
  front.active = false;
  front.zOrder = 0;

  mgr.pushCommand(
      std::make_unique<LambdaCommand>(
          "Delete Node",
          [&front]() {
            front.active = false;
            front.zOrder = 0;
          },
          [&front]() {
            front.active = true;
            front.zOrder = 5;
          }));

  EXPECT_FALSE(front.active);
  EXPECT_EQ(0, front.zOrder);
  EXPECT_TRUE(back.active);
  EXPECT_EQ(2, back.zOrder);

  // Undo: front node restored with original z-order
  mgr.undo();
  EXPECT_TRUE(front.active);
  EXPECT_EQ(5, front.zOrder);
  EXPECT_TRUE(back.active);
  EXPECT_EQ(2, back.zOrder);

  // Redo: front node deleted again
  mgr.redo();
  EXPECT_FALSE(front.active);
}

TEST_F(NoodlesUndoManagerTest, MultipleOperationsPreserveState) {
  auto& mgr = NoodlesUndoManager::instance();
  int a = 1, b = 10;

  // Operation 1: set a from 1 to 2
  a = 2;
  mgr.pushCommand(std::make_unique<LambdaCommand>("Set A", [&a]() { a = 2; }, [&a]() { a = 1; }));

  // Operation 2: set b from 10 to 20
  b = 20;
  mgr.pushCommand(std::make_unique<LambdaCommand>("Set B", [&b]() { b = 20; }, [&b]() { b = 10; }));

  EXPECT_EQ(2, a);
  EXPECT_EQ(20, b);

  // Undo op2: b restored, a unchanged
  mgr.undo();
  EXPECT_EQ(2, a);
  EXPECT_EQ(10, b);

  // Undo op1: both restored
  mgr.undo();
  EXPECT_EQ(1, a);
  EXPECT_EQ(10, b);

  // Redo op1: a re-applied
  mgr.redo();
  EXPECT_EQ(2, a);
  EXPECT_EQ(10, b);

  // Redo op2: b re-applied
  mgr.redo();
  EXPECT_EQ(2, a);
  EXPECT_EQ(20, b);
}

// --- Mixed connections and nodes test ---

TEST_F(NoodlesUndoManagerTest, BatchDeleteMixedNodesAndConnections) {
  auto& mgr = NoodlesUndoManager::instance();

  bool conn1 = true, conn2 = true;
  bool node1 = true, node2 = true;

  // Delete everything
  conn1 = false;
  conn2 = false;
  node1 = false;
  node2 = false;

  mgr.pushCommand(
      std::make_unique<LambdaCommand>(
          "Delete Selected",
          [&]() {
            conn1 = false;
            conn2 = false;
            node1 = false;
            node2 = false;
          },
          [&]() {
            conn1 = true;
            conn2 = true;
            node1 = true;
            node2 = true;
          }));

  EXPECT_FALSE(conn1);
  EXPECT_FALSE(conn2);
  EXPECT_FALSE(node1);
  EXPECT_FALSE(node2);

  // Single undo restores everything
  mgr.undo();
  EXPECT_TRUE(conn1);
  EXPECT_TRUE(conn2);
  EXPECT_TRUE(node1);
  EXPECT_TRUE(node2);

  // Redo re-deletes everything
  mgr.redo();
  EXPECT_FALSE(conn1);
  EXPECT_FALSE(conn2);
  EXPECT_FALSE(node1);
  EXPECT_FALSE(node2);
}

} // namespace noodles
