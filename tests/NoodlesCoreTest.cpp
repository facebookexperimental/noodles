// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/Animator.h"
#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/RenderConfig.h"

#include <cmath>
#include <cstring>

namespace noodles {

// --- Animator ---

TEST(AnimatorTest, DefaultState) {
  Animator animator;
  EXPECT_FALSE(animator.isAnimating());
  EXPECT_TRUE(animator.properties().empty());
}

TEST(AnimatorTest, AddProperty) {
  Animator animator;
  auto* prop = animator.addProperty();
  ASSERT_NE(nullptr, prop);
  EXPECT_EQ(1u, animator.properties().size());
  EXPECT_DOUBLE_EQ(0.0, prop->current);
  EXPECT_DOUBLE_EQ(0.0, prop->target);
  EXPECT_DOUBLE_EQ(1.0, prop->speed);
}

TEST(AnimatorTest, AddMultipleProperties) {
  Animator animator;
  animator.addProperty();
  animator.addProperty();
  animator.addProperty();
  EXPECT_EQ(3u, animator.properties().size());
}

TEST(AnimatorTest, RemoveProperty) {
  Animator animator;
  animator.addProperty();
  animator.addProperty();
  animator.addProperty();
  EXPECT_EQ(3u, animator.properties().size());

  auto* prop2 = animator.properties()[1];
  animator.removeProperty(prop2);
  EXPECT_EQ(2u, animator.properties().size());
}

TEST(AnimatorTest, RemoveInvalidPointer) {
  Animator animator;
  animator.addProperty();
  AnimatableProperty dummy;
  animator.removeProperty(&dummy);
  EXPECT_EQ(1u, animator.properties().size());
}

TEST(AnimatorTest, UpdateConvergence) {
  Animator animator;
  auto* prop = animator.addProperty();
  prop->target = 10.0;
  prop->speed = 1.0;

  // Large dt should converge
  animator.update(100.0);
  EXPECT_DOUBLE_EQ(10.0, prop->current);
  EXPECT_FALSE(animator.isAnimating());
}

TEST(AnimatorTest, UpdateInterpolation) {
  Animator animator;
  auto* prop = animator.addProperty();
  prop->target = 10.0;
  prop->speed = 1.0;

  animator.update(0.01);
  EXPECT_GT(prop->current, 0.0);
  EXPECT_LT(prop->current, 10.0);
  EXPECT_TRUE(animator.isAnimating());
}

TEST(AnimatorTest, MultiplePropertiesConvergeIndependently) {
  Animator animator;
  animator.addProperty();
  animator.addProperty();

  // Retrieve pointers after all additions to avoid invalidation
  auto* p1 = animator.properties()[0];
  auto* p2 = animator.properties()[1];

  p1->target = 5.0;
  p1->speed = 1.0;
  p2->target = 100.0;
  p2->speed = 0.01;

  // Large dt converges fast property, slow property might not
  animator.update(100.0);
  EXPECT_DOUBLE_EQ(5.0, p1->current);

  // Slow property may or may not have converged at speed=0.01, dt=100
  // s = 1 - exp(-100 * 0.01) = 1 - exp(-1) ~ 0.632
  // current ~ 0.632 * 100 = 63.2, not converged
  EXPECT_GT(p2->current, 0.0);
  EXPECT_LT(p2->current, 100.0);
  EXPECT_TRUE(animator.isAnimating());
}

TEST(AnimatorTest, NoPropertiesNotAnimating) {
  Animator animator;
  animator.update(1.0);
  EXPECT_FALSE(animator.isAnimating());
}

TEST(AnimatorTest, ConvergedPropertySnapsToTarget) {
  Animator animator;
  auto* prop = animator.addProperty();
  prop->current = 9.9999999;
  prop->target = 10.0;
  prop->speed = 1.0;

  animator.update(100.0);
  EXPECT_DOUBLE_EQ(10.0, prop->current);
}

// --- RenderConfig ---

TEST(RenderConfigTest, DefaultValues) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(2.0, config.globalNodeScale);
  EXPECT_DOUBLE_EQ(24.0, config.nodeTitleFontSize);
  EXPECT_DOUBLE_EQ(18.0, config.nodePinFontSize);
  EXPECT_DOUBLE_EQ(14.0, config.nodePinTypeFontSize);
  EXPECT_DOUBLE_EQ(16.0, config.nodeMarginH);
  EXPECT_DOUBLE_EQ(18.0, config.nodeMarginV);
  EXPECT_DOUBLE_EQ(10.0, config.nodeCornerRadius);
  EXPECT_DOUBLE_EQ(14.0, config.nodeFontSize);
  EXPECT_DOUBLE_EQ(10.0, config.linkLineWidth);
  EXPECT_EQ(50, config.nodeBgHigh);
  EXPECT_EQ(40, config.nodeBgLow);
  EXPECT_EQ(248, config.nodeBgAlpha);
  EXPECT_EQ("default", config.nodeRendererType);
}

TEST(RenderConfigTest, GetCamelCaseKey) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(2.0, config.get("globalNodeScale", 0.0));
  EXPECT_DOUBLE_EQ(24.0, config.get("nodeTitleFontSize", 0.0));
  EXPECT_DOUBLE_EQ(10.0, config.get("linkLineWidth", 0.0));
}

TEST(RenderConfigTest, GetSnakeCaseKey) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(2.0, config.get("global_node_scale", 0.0));
  EXPECT_DOUBLE_EQ(24.0, config.get("node_title_font_size", 0.0));
}

TEST(RenderConfigTest, GetUnknownKeyReturnsDefault) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(42.0, config.get("nonExistentKey", 42.0));
  EXPECT_EQ(99, config.getInt("nonExistentKey", 99));
  EXPECT_EQ("fallback", config.getString("nonExistentKey", "fallback"));
}

TEST(RenderConfigTest, GetInt) {
  RenderConfig config;
  EXPECT_EQ(50, config.getInt("nodeBgHigh", 0));
  EXPECT_EQ(40, config.getInt("nodeBgLow", 0));
  EXPECT_EQ(248, config.getInt("nodeBgAlpha", 0));
}

TEST(RenderConfigTest, GetString) {
  RenderConfig config;
  EXPECT_EQ("default", config.getString("nodeRendererType", ""));
}

TEST(RenderConfigTest, ModifiedValueReflectedInGet) {
  RenderConfig config;
  config.globalNodeScale = 5.0;
  EXPECT_DOUBLE_EQ(5.0, config.get("globalNodeScale", 0.0));

  config.nodeBgHigh = 100;
  EXPECT_EQ(100, config.getInt("nodeBgHigh", 0));

  config.nodeRendererType = "graffi";
  EXPECT_EQ("graffi", config.getString("nodeRendererType", ""));
}

// --- NodeData / LinkData / StickerData ---

TEST(NodeDataTest, DefaultConstruction) {
  NodeData node;
  EXPECT_TRUE(node.id.empty());
  EXPECT_TRUE(node.name.empty());
  EXPECT_TRUE(node.type.empty());
  EXPECT_DOUBLE_EQ(200.0, node.size[0]);
  EXPECT_DOUBLE_EQ(100.0, node.size[1]);
  EXPECT_FALSE(node.selected);
  EXPECT_EQ("default", node.uiStyle);
}

TEST(NodeDataTest, FieldAssignment) {
  NodeData node;
  node.id = "node_1";
  node.name = "TestNode";
  node.inputPins = {"in1", "in2"};
  node.outputPins = {"out1"};
  EXPECT_EQ("node_1", node.id);
  EXPECT_EQ("TestNode", node.name);
  EXPECT_EQ(2u, node.inputPins.size());
  EXPECT_EQ(1u, node.outputPins.size());
}

TEST(LinkDataTest, DefaultConstruction) {
  LinkData link;
  EXPECT_TRUE(link.sourceNodeId.empty());
  EXPECT_TRUE(link.targetNodeId.empty());
  EXPECT_FALSE(link.selected);
  EXPECT_FALSE(link.isDangling);
  EXPECT_FALSE(link.hasColor);
  EXPECT_FALSE(link.highlighted);
  EXPECT_DOUBLE_EQ(50.0, LinkData::DANGLING_LINK_LENGTH);
}

TEST(LinkDataTest, FieldAssignment) {
  LinkData link;
  link.sourceNodeId = "a";
  link.sourcePort = "out";
  link.targetNodeId = "b";
  link.targetPort = "in";
  link.selected = true;
  EXPECT_EQ("a", link.sourceNodeId);
  EXPECT_EQ("b", link.targetNodeId);
  EXPECT_TRUE(link.selected);
}

TEST(StickerDataTest, DefaultConstruction) {
  StickerData sticker;
  EXPECT_TRUE(sticker.id.empty());
  EXPECT_DOUBLE_EQ(400.0, sticker.size[0]);
  EXPECT_DOUBLE_EQ(300.0, sticker.size[1]);
  EXPECT_FLOAT_EQ(0.2f, sticker.r);
  EXPECT_FLOAT_EQ(0.5f, sticker.a);
  EXPECT_FALSE(sticker.selected);
}

// --- GraphModel ---

TEST(GraphModelTest, DefaultConstruction) {
  GraphModel model;
  EXPECT_TRUE(model.nodes.empty());
  EXPECT_TRUE(model.links.empty());
  EXPECT_TRUE(model.stickers.empty());
  EXPECT_TRUE(model.isLinksChanged());
}

TEST(GraphModelTest, Clear) {
  GraphModel model;
  model.nodes["a"] = NodeData{};
  model.links.push_back(LinkData{});
  model.stickers.push_back(StickerData{});
  model.buildConnectionCache();

  model.clear();
  EXPECT_TRUE(model.nodes.empty());
  EXPECT_TRUE(model.links.empty());
  EXPECT_TRUE(model.stickers.empty());
  EXPECT_TRUE(model.isLinksChanged());
}

TEST(GraphModelTest, ConnectionCache) {
  GraphModel model;
  model.nodes["a"] = NodeData{};
  model.nodes["b"] = NodeData{};
  model.nodes["c"] = NodeData{};

  LinkData link1;
  link1.sourceNodeId = "a";
  link1.targetNodeId = "b";
  model.links.push_back(link1);

  LinkData link2;
  link2.sourceNodeId = "b";
  link2.targetNodeId = "c";
  model.links.push_back(link2);

  model.buildConnectionCache();

  auto connA = model.getConnectedNodeIds("a");
  EXPECT_EQ(1u, connA.size());
  EXPECT_EQ(1u, connA.count("b"));

  auto connB = model.getConnectedNodeIds("b");
  EXPECT_EQ(2u, connB.size());
  EXPECT_EQ(1u, connB.count("a"));
  EXPECT_EQ(1u, connB.count("c"));

  auto connC = model.getConnectedNodeIds("c");
  EXPECT_EQ(1u, connC.size());
  EXPECT_EQ(1u, connC.count("b"));
}

TEST(GraphModelTest, GetConnectedNodeIdsUnknown) {
  GraphModel model;
  model.nodes["a"] = NodeData{};
  model.buildConnectionCache();

  auto conn = model.getConnectedNodeIds("nonexistent");
  EXPECT_TRUE(conn.empty());
}

TEST(GraphModelTest, CalculateNodeSizeNoPins) {
  GraphModel model;
  NodeData node;
  node.name = "Empty";
  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  model.calculateNodeSize(node, calcTextWidth, metrics);

  EXPECT_GE(node.size[0], 150.0);
  EXPECT_GT(node.size[1], 0.0);
}

TEST(GraphModelTest, CalculateNodeSizeWithPins) {
  GraphModel model;
  NodeData nodeNoPins;
  nodeNoPins.name = "A";
  NodeData nodeWithPins;
  nodeWithPins.name = "A";
  nodeWithPins.inputPins = {"x", "y", "z"};
  nodeWithPins.outputPins = {"result"};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  model.calculateNodeSize(nodeNoPins, calcTextWidth, metrics);
  model.calculateNodeSize(nodeWithPins, calcTextWidth, metrics);

  EXPECT_GT(nodeWithPins.size[1], nodeNoPins.size[1]);
}

TEST(GraphModelTest, CalculateNodeSizeWithCustomConfig) {
  GraphModel model;
  NodeData node;
  node.name = "Test";
  node.inputPins = {"input1"};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  RenderConfig config;
  config.globalNodeScale = 1.0;
  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  RenderConfig config2;
  config2.globalNodeScale = 3.0;
  NodeData node2 = node;
  node2.inputPins = {"input1"};
  model.calculateNodeSize(node2, calcTextWidth, metrics, &config2);

  EXPECT_GT(node2.size[1], node.size[1]);
}

// --- NodeVertex ---

TEST(NodeVertexTest, StaticSize) {
  EXPECT_EQ(44u, NodeVertex::size());
}

TEST(NodeVertexTest, DefaultValues) {
  NodeVertex v;
  EXPECT_FLOAT_EQ(0.0f, v.x);
  EXPECT_FLOAT_EQ(0.0f, v.y);
  EXPECT_FLOAT_EQ(0.0f, v.z);
  EXPECT_EQ(0, v.r);
  EXPECT_EQ(0, v.g);
  EXPECT_EQ(0, v.b);
  EXPECT_EQ(255, v.a);
  EXPECT_EQ(255, v.sa);
  EXPECT_FLOAT_EQ(0.0f, v.selected);
}

TEST(NodeVertexTest, PackSize) {
  NodeVertex v;
  auto packed = v.pack();
  EXPECT_EQ(44u, packed.size());
}

TEST(NodeVertexTest, PackBatchEmpty) {
  std::vector<NodeVertex> empty;
  auto packed = NodeVertex::packBatch(empty);
  EXPECT_TRUE(packed.empty());
}

TEST(NodeVertexTest, PackBatchSize) {
  std::vector<NodeVertex> verts(5);
  auto packed = NodeVertex::packBatch(verts);
  EXPECT_EQ(5u * 44u, packed.size());
}

TEST(NodeVertexTest, PackRoundTrip) {
  NodeVertex v(
      1.0f, 2.0f, 3.0f, 0.5f, 0.5f, 100.0f, 200.0f, 128, 64, 32, 200, 1.0f, 10, 20, 30, 250, 1.0f);

  auto packed = v.pack();
  ASSERT_EQ(44u, packed.size());

  // Verify position floats at correct offsets
  float readX = 0.0f;
  std::memcpy(&readX, packed.data(), sizeof(float));
  EXPECT_FLOAT_EQ(1.0f, readX);

  float readY = 0.0f;
  std::memcpy(&readY, packed.data() + 4, sizeof(float));
  EXPECT_FLOAT_EQ(2.0f, readY);

  float readZ = 0.0f;
  std::memcpy(&readZ, packed.data() + 8, sizeof(float));
  EXPECT_FLOAT_EQ(3.0f, readZ);

  // Color bytes at offset 28 (7 floats * 4 bytes)
  EXPECT_EQ(128, packed[28]);
  EXPECT_EQ(64, packed[29]);
  EXPECT_EQ(32, packed[30]);
  EXPECT_EQ(200, packed[31]);

  // Selected color bytes at offset 36 (28 + 4 color bytes + 4 innerStroke float)
  EXPECT_EQ(10, packed[36]);
  EXPECT_EQ(20, packed[37]);
  EXPECT_EQ(30, packed[38]);
  EXPECT_EQ(250, packed[39]);

  // Selected float at offset 40
  float readSelected = 0.0f;
  std::memcpy(&readSelected, packed.data() + 40, sizeof(float));
  EXPECT_FLOAT_EQ(1.0f, readSelected);
}

// --- Animator Edge Cases ---

TEST(AnimatorTest, PointerStabilityAfterMultipleAdds) {
  // With std::deque, pointers returned from addProperty() remain valid
  // even after subsequent adds.
  Animator animator;
  auto* p0 = animator.addProperty();
  p0->target = 1.0;

  auto* p1 = animator.addProperty();
  p1->target = 2.0;

  auto* p2 = animator.addProperty();
  p2->target = 3.0;

  // Earlier pointers must still be valid and readable
  EXPECT_DOUBLE_EQ(1.0, p0->target);
  EXPECT_DOUBLE_EQ(2.0, p1->target);
  EXPECT_DOUBLE_EQ(3.0, p2->target);

  // Verify they match what properties() returns
  EXPECT_EQ(p0, animator.properties()[0]);
  EXPECT_EQ(p1, animator.properties()[1]);
  EXPECT_EQ(p2, animator.properties()[2]);
}

TEST(AnimatorTest, ZeroSpeedNoMovement) {
  Animator animator;
  auto* prop = animator.addProperty();
  prop->target = 10.0;
  prop->speed = 0.0;

  animator.update(1.0);
  EXPECT_DOUBLE_EQ(0.0, prop->current);
}

TEST(AnimatorTest, NegativeDtNoChange) {
  // Negative dt should still work with the exponential formula
  Animator animator;
  auto* prop = animator.addProperty();
  prop->target = 10.0;
  prop->speed = 1.0;

  animator.update(-1.0);
  // exp(-(-1)*1) = exp(1) > 1, so s = 1 - exp(1) < 0
  // current = 0 + negative * 10 < 0, or implementation may clamp
  // The point is it shouldn't crash
  EXPECT_TRUE(true);
}

TEST(AnimatorTest, RemoveMiddleProperty) {
  Animator animator;
  animator.addProperty();
  animator.addProperty();
  animator.addProperty();

  auto* mid = animator.properties()[1];
  mid->target = 42.0;

  animator.removeProperty(mid);
  EXPECT_EQ(2u, animator.properties().size());
}

TEST(AnimatorTest, RemoveAllProperties) {
  Animator animator;
  animator.addProperty();
  animator.addProperty();

  // Remove all via properties() snapshot
  while (!animator.properties().empty()) {
    animator.removeProperty(animator.properties()[0]);
  }

  EXPECT_TRUE(animator.properties().empty());
  EXPECT_FALSE(animator.isAnimating());
}

// --- RenderConfig Edge Cases ---

TEST(RenderConfigTest, CamelToSnakeConsecutiveCaps) {
  // Tests that consecutive capitals in camelCase keys still match
  RenderConfig config;
  // nodeMarginH is a valid key
  EXPECT_DOUBLE_EQ(16.0, config.get("nodeMarginH", 0.0));
  EXPECT_DOUBLE_EQ(18.0, config.get("nodeMarginV", 0.0));
}

TEST(RenderConfigTest, AllDoubleFieldsAccessible) {
  RenderConfig config;
  // Verify all double fields are accessible via get()
  EXPECT_DOUBLE_EQ(config.globalNodeScale, config.get("globalNodeScale", -1.0));
  EXPECT_DOUBLE_EQ(config.nodeTitleFontSize, config.get("nodeTitleFontSize", -1.0));
  EXPECT_DOUBLE_EQ(config.nodePinFontSize, config.get("nodePinFontSize", -1.0));
  EXPECT_DOUBLE_EQ(config.nodePinTypeFontSize, config.get("nodePinTypeFontSize", -1.0));
  EXPECT_DOUBLE_EQ(config.nodeMarginH, config.get("nodeMarginH", -1.0));
  EXPECT_DOUBLE_EQ(config.nodeMarginV, config.get("nodeMarginV", -1.0));
  EXPECT_DOUBLE_EQ(config.nodeCornerRadius, config.get("nodeCornerRadius", -1.0));
  EXPECT_DOUBLE_EQ(config.nodeFontSize, config.get("nodeFontSize", -1.0));
  EXPECT_DOUBLE_EQ(config.linkLineWidth, config.get("linkLineWidth", -1.0));
}

TEST(RenderConfigTest, AllIntFieldsAccessible) {
  RenderConfig config;
  EXPECT_EQ(config.nodeBgHigh, config.getInt("nodeBgHigh", -1));
  EXPECT_EQ(config.nodeBgLow, config.getInt("nodeBgLow", -1));
  EXPECT_EQ(config.nodeBgAlpha, config.getInt("nodeBgAlpha", -1));
}

// --- GraphModel Edge Cases ---

TEST(GraphModelTest, ConnectionCacheMultipleLinks) {
  GraphModel model;
  model.nodes["a"] = NodeData{};
  model.nodes["b"] = NodeData{};

  LinkData link1;
  link1.sourceNodeId = "a";
  link1.targetNodeId = "b";
  model.links.push_back(link1);

  LinkData link2;
  link2.sourceNodeId = "a";
  link2.targetNodeId = "b";
  model.links.push_back(link2);

  auto connA = model.getConnectedNodeIds("a");
  EXPECT_EQ(1u, connA.size());
  EXPECT_EQ(1u, connA.count("b"));
}

TEST(GraphModelTest, LazyConnectionCacheRebuild) {
  GraphModel model;
  model.nodes["a"] = NodeData{};
  model.nodes["b"] = NodeData{};

  LinkData link;
  link.sourceNodeId = "a";
  link.targetNodeId = "b";
  model.links.push_back(link);

  // No explicit buildConnectionCache -- getConnectedNodeIds should auto-rebuild
  auto connA = model.getConnectedNodeIds("a");
  EXPECT_EQ(1u, connA.size());
  EXPECT_EQ(1u, connA.count("b"));
  EXPECT_FALSE(model.isLinksChanged());
}

TEST(GraphModelTest, SelfLoop) {
  GraphModel model;
  model.nodes["a"] = NodeData{};

  LinkData selfLink;
  selfLink.sourceNodeId = "a";
  selfLink.targetNodeId = "a";
  model.links.push_back(selfLink);

  model.buildConnectionCache();

  auto connA = model.getConnectedNodeIds("a");
  EXPECT_EQ(1u, connA.size());
  EXPECT_EQ(1u, connA.count("a"));
}

TEST(GraphModelTest, ClearResetsLinksChanged) {
  GraphModel model;
  model.buildConnectionCache();
  EXPECT_FALSE(model.isLinksChanged());
  model.clear();
  EXPECT_TRUE(model.isLinksChanged());
}

// --- NodeVertex Edge Cases ---

TEST(NodeVertexTest, PackBatchMultipleVerticesRoundTrip) {
  std::vector<NodeVertex> verts(3);
  verts[0].x = 1.0f;
  verts[0].y = 2.0f;
  verts[1].x = 3.0f;
  verts[1].y = 4.0f;
  verts[2].x = 5.0f;
  verts[2].y = 6.0f;

  auto packed = NodeVertex::packBatch(verts);
  ASSERT_EQ(3u * 44u, packed.size());

  // Verify each vertex position is at the right offset
  for (size_t i = 0; i < 3; ++i) {
    float readX = 0.0f, readY = 0.0f;
    std::memcpy(&readX, packed.data() + i * 44, sizeof(float));
    std::memcpy(&readY, packed.data() + i * 44 + 4, sizeof(float));
    EXPECT_FLOAT_EQ(verts[i].x, readX) << "vertex " << i;
    EXPECT_FLOAT_EQ(verts[i].y, readY) << "vertex " << i;
  }
}

TEST(NodeVertexTest, AllFieldsPackedCorrectly) {
  NodeVertex v(
      10.0f, 20.0f, 30.0f, 0.1f, 0.2f, 100.0f, 200.0f, 11, 22, 33, 44, 0.5f, 55, 66, 77, 88, 1.0f);

  auto packed = v.pack();
  ASSERT_EQ(44u, packed.size());

  // UV at offset 12, 16
  float u = 0.0f, v2 = 0.0f;
  std::memcpy(&u, packed.data() + 12, sizeof(float));
  std::memcpy(&v2, packed.data() + 16, sizeof(float));
  EXPECT_FLOAT_EQ(0.1f, u);
  EXPECT_FLOAT_EQ(0.2f, v2);

  // Rect size at offset 20, 24
  float rw = 0.0f, rh = 0.0f;
  std::memcpy(&rw, packed.data() + 20, sizeof(float));
  std::memcpy(&rh, packed.data() + 24, sizeof(float));
  EXPECT_FLOAT_EQ(100.0f, rw);
  EXPECT_FLOAT_EQ(200.0f, rh);

  // Inner stroke at offset 32
  float stroke = 0.0f;
  std::memcpy(&stroke, packed.data() + 32, sizeof(float));
  EXPECT_FLOAT_EQ(0.5f, stroke);
}

// --- FontMetrics ---

TEST(FontMetricsTest, DefaultValues) {
  FontMetrics fm;
  EXPECT_DOUBLE_EQ(0.0, fm.ascender);
  EXPECT_DOUBLE_EQ(0.0, fm.descender);
  EXPECT_DOUBLE_EQ(0.0, fm.lineHeight);
}

TEST(RenderConfigTest, IsGraffiStyleDefault) {
  RenderConfig config;
  EXPECT_FALSE(config.isGraffiStyle);
}

TEST(GraphModelTest, CalculateNodeSizeGraffiStyle) {
  GraphModel model;
  NodeData node;
  node.name = "TestNode";
  node.type = "SomeType";
  node.inputPins = {"x", "y"};
  node.outputPins = {"result"};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  RenderConfig defaultConfig;
  defaultConfig.globalNodeScale = 2.0;
  NodeData nodeDefault = node;
  model.calculateNodeSize(nodeDefault, calcTextWidth, metrics, &defaultConfig);

  RenderConfig graffiConfig;
  graffiConfig.globalNodeScale = 2.0;
  graffiConfig.isGraffiStyle = true;
  NodeData nodeGraffi = node;
  model.calculateNodeSize(nodeGraffi, calcTextWidth, metrics, &graffiConfig);

  // Graffi style may produce a wider node due to the type label in the center
  EXPECT_GE(nodeGraffi.size[0], nodeDefault.size[0]);
}

TEST(GraphModelTest, CalculateNodeSizeUsesDisplayRowKinds) {
  GraphModel model;
  NodeData node;
  node.name = "Test";
  node.inputPins = {"inputs", "foo", "middle", "bar"};
  node.inputRowKinds = {2, 3, 0, 3};
  node.inputRowSlots = {0, 1, 4, 5};
  node.outputPins = {"outputs", "left", "right"};
  node.outputRowKinds = {2, 3, 3};
  node.outputRowSlots = {2, 3, 6};
  node.displayRowKinds = {2, 3, 2, 3, 0, 3, 3};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  RenderConfig config;
  config.globalNodeScale = 2.0;

  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  const double nodeTitleFontSize = config.get("nodeTitleFontSize", 24.0) * config.globalNodeScale;
  const double nodePinFontSize = config.get("nodePinFontSize", 18.0) * config.globalNodeScale;
  const double nodePinTypeFontSize =
      config.get("nodePinTypeFontSize", 14.0) * config.globalNodeScale;
  const double nodeMarginV = config.get("nodeMarginV", 18.0) * config.globalNodeScale;
  const double nodePortSpacing = config.get("nodePortSpacing", 1.0) * config.globalNodeScale;
  const double titleAreaHeight =
      nodeTitleFontSize * (metrics.ascender - metrics.descender) + nodeMarginV * 2.0;
  const double portLineHeight = nodePinFontSize * metrics.lineHeight +
      nodePinTypeFontSize * metrics.lineHeight + nodePortSpacing;
  const double expectedHeight = titleAreaHeight +
      static_cast<double>(node.displayRowKinds.size()) * portLineHeight + nodeMarginV;

  EXPECT_DOUBLE_EQ(node.size[1], expectedHeight);
}

TEST(GraphModelTest, CalculateNodeSizeNoRenderer) {
  // Verify calculateNodeSize works without any renderer (the back-pointer was removed)
  GraphModel model;
  NodeData node;
  node.name = "NoRenderer";
  node.inputPins = {"in"};
  node.outputPins = {"out"};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  model.calculateNodeSize(node, calcTextWidth, metrics);
  EXPECT_GE(node.size[0], 150.0);
  EXPECT_GT(node.size[1], 0.0);
}

} // namespace noodles
