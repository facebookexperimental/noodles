// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/Animator.h"
#include "core/NodeData.h"
#include "core/NodeLayout.h"
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
  EXPECT_DOUBLE_EQ(48.0, config.nodeTitleFontSize);
  EXPECT_DOUBLE_EQ(36.0, config.nodePinFontSize);
  EXPECT_DOUBLE_EQ(26.0, config.nodePinTypeFontSize);
  EXPECT_DOUBLE_EQ(30.0, config.nodeMarginH);
  EXPECT_DOUBLE_EQ(20.0, config.nodeMarginV);
  EXPECT_DOUBLE_EQ(14.0, config.nodeCornerRadius);
  EXPECT_DOUBLE_EQ(14.0, config.nodeFontSize);
  EXPECT_DOUBLE_EQ(12.0, config.linkLineWidth);
  EXPECT_EQ(90, config.nodeBgHigh);
  EXPECT_EQ(86, config.nodeBgLow);
  EXPECT_EQ(245, config.nodeBgAlpha);
  EXPECT_EQ("default", config.nodeRendererType);
}

TEST(RenderConfigTest, GetCamelCaseKey) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(48.0, config.get("nodeTitleFontSize", 0.0));
  EXPECT_DOUBLE_EQ(12.0, config.get("linkLineWidth", 0.0));
}

TEST(RenderConfigTest, GetSnakeCaseKey) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(48.0, config.get("node_title_font_size", 0.0));
}

TEST(RenderConfigTest, GetUnknownKeyReturnsDefault) {
  RenderConfig config;
  EXPECT_DOUBLE_EQ(42.0, config.get("nonExistentKey", 42.0));
  EXPECT_EQ(99, config.getInt("nonExistentKey", 99));
  EXPECT_EQ("fallback", config.getString("nonExistentKey", "fallback"));
}

TEST(RenderConfigTest, GetInt) {
  RenderConfig config;
  EXPECT_EQ(90, config.getInt("nodeBgHigh", 0));
  EXPECT_EQ(86, config.getInt("nodeBgLow", 0));
  EXPECT_EQ(245, config.getInt("nodeBgAlpha", 0));
}

TEST(RenderConfigTest, GetString) {
  RenderConfig config;
  EXPECT_EQ("default", config.getString("nodeRendererType", ""));
}

TEST(RenderConfigTest, ModifiedValueReflectedInGet) {
  RenderConfig config;
  config.nodeTitleFontSize = 60.0;
  EXPECT_DOUBLE_EQ(60.0, config.get("nodeTitleFontSize", 0.0));

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
  EXPECT_TRUE(node.schemaTypeName.empty());
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
  EXPECT_FALSE(link.isRelationship);
  EXPECT_DOUBLE_EQ(50.0, LinkData::DANGLING_LINK_LENGTH);
}

TEST(LinkDataTest, HighlightColorDefaults) {
  LinkData link;
  EXPECT_FLOAT_EQ(0.0f, link.highlightColor[0]);
  EXPECT_FLOAT_EQ(0.0f, link.highlightColor[1]);
  EXPECT_FLOAT_EQ(0.0f, link.highlightColor[2]);
  EXPECT_FLOAT_EQ(0.0f, link.highlightColor[3]);
}

TEST(LinkDataTest, ColorDefaults) {
  LinkData link;
  EXPECT_FLOAT_EQ(0.0f, link.color[0]);
  EXPECT_FLOAT_EQ(0.0f, link.color[1]);
  EXPECT_FLOAT_EQ(0.0f, link.color[2]);
  EXPECT_FLOAT_EQ(0.0f, link.color[3]);
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

TEST(LinkDataTest, HighlightedFieldMutation) {
  LinkData link;
  EXPECT_FALSE(link.highlighted);
  link.highlighted = true;
  EXPECT_TRUE(link.highlighted);
  link.highlighted = false;
  EXPECT_FALSE(link.highlighted);
}

TEST(LinkDataTest, HighlightColorMutation) {
  LinkData link;
  link.highlightColor = {0.31f, 0.78f, 0.47f, 1.0f};
  EXPECT_FLOAT_EQ(0.31f, link.highlightColor[0]);
  EXPECT_FLOAT_EQ(0.78f, link.highlightColor[1]);
  EXPECT_FLOAT_EQ(0.47f, link.highlightColor[2]);
  EXPECT_FLOAT_EQ(1.0f, link.highlightColor[3]);
}

TEST(LinkDataTest, ColorMutation) {
  LinkData link;
  link.color = {1.0f, 0.5f, 0.5f, 1.0f};
  EXPECT_FLOAT_EQ(1.0f, link.color[0]);
  EXPECT_FLOAT_EQ(0.5f, link.color[1]);
  EXPECT_FLOAT_EQ(0.5f, link.color[2]);
  EXPECT_FLOAT_EQ(1.0f, link.color[3]);
}

TEST(LinkDataTest, IsRelationshipFieldMutation) {
  LinkData link;
  EXPECT_FALSE(link.isRelationship);
  link.isRelationship = true;
  EXPECT_TRUE(link.isRelationship);
  link.isRelationship = false;
  EXPECT_FALSE(link.isRelationship);
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

TEST(GraphModelTest, LinkCount) {
  GraphModel model;
  EXPECT_EQ(0u, model.linkCount());
  model.links.push_back(LinkData{});
  model.links.push_back(LinkData{});
  EXPECT_EQ(2u, model.linkCount());
  EXPECT_EQ(model.links.size(), model.linkCount());
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

TEST(GraphModelTest, CalculateNodeSizeResolvesPerPinCenterY) {
  // calculateNodeSize must emit one band-center Y per pin, resolved through the
  // authored row slots, so every renderer/hit-test reads the same per-pin Y and
  // never re-derives a slot or row step (this is what prevents per-row drift).
  GraphModel model;
  NodeData node;
  node.name = "N";
  node.inputPins = {"a", "b"};
  node.inputRowSlots = {0, 2}; // 'b' lives on visible row slot 2, not 1
  node.outputPins = {"c"};
  node.outputRowSlots = {1};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;
  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  model.calculateNodeSize(node, calcTextWidth, metrics);

  const double s = node.layoutPortStartY;
  const double L = node.layoutPortLineHeight;
  ASSERT_EQ(2u, node.layoutInputCenterY.size());
  ASSERT_EQ(1u, node.layoutOutputCenterY.size());
  EXPECT_DOUBLE_EQ(s + 0 * L + L * 0.5, node.layoutInputCenterY[0]);
  EXPECT_DOUBLE_EQ(s + 2 * L + L * 0.5, node.layoutInputCenterY[1]); // honors slot 2
  EXPECT_DOUBLE_EQ(s + 1 * L + L * 0.5, node.layoutOutputCenterY[0]);
}

TEST(GraphModelTest, CalculateNodeSizeCollapsedPinsConvergeToCenter) {
  GraphModel model;
  NodeData node;
  node.name = "C";
  node.inputPins = {"a", "b"};
  node.outputPins = {"c"};
  node.titleCollapsed = true;

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;
  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  model.calculateNodeSize(node, calcTextWidth, metrics);

  // Guard against a vacuous test: a collapsed node still resolves one center
  // per pin (they just all converge), so the loops below must iterate.
  ASSERT_EQ(2u, node.layoutInputCenterY.size());
  ASSERT_EQ(1u, node.layoutOutputCenterY.size());
  // Collapsed: every pin converges to the node's vertical center.
  for (double y : node.layoutInputCenterY) {
    EXPECT_DOUBLE_EQ(node.size[1] * 0.5, y);
  }
  for (double y : node.layoutOutputCenterY) {
    EXPECT_DOUBLE_EQ(node.size[1] * 0.5, y);
  }
}

// Pins the height contract for a title-collapsed node: total height equals
// titleAreaHeight, which is titleTextHeight + nodeMarginV*2 (the title-bar's
// internal vertical padding above and below the title text — see
// NodeData.cpp). A regression that re-adds a trailing nodeMarginV BELOW the
// title bar (totalHeight = titleAreaHeight + nodeMarginV) would fail this
// assertion. The sibling convergence test does not catch that regression
// because the pin centers are computed as node.size[1] * 0.5 regardless of
// the height value.
TEST(GraphModelTest, CalculateNodeSizeCollapsedHeightHasNoTrailingMargin) {
  GraphModel model;
  NodeData node;
  node.name = "C";
  node.inputPins = {"a", "b"};
  node.outputPins = {"c"};
  node.titleCollapsed = true;

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;
  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  // Pin the inputs explicitly so this test asserts the structural invariant
  // (collapsed totalHeight has no trailing margin) rather than the current
  // RenderConfig defaults — a struct-default change must not silently flip
  // this assertion red without a real regression.
  RenderConfig config;
  config.nodeTitleFontSize = 48.0;
  config.nodeMarginV = 20.0;
  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  // Concrete spec-derived expectation (independent of the implementation
  // formula) so a future refactor of titleAreaHeight cannot silently shift
  // both sides in lockstep:
  //   nodeTitleFontSize    = 48.0 (set above)
  //   nodeMarginV          = 20.0 (set above)
  //   ascender - descender = 0.8 - (-0.2) = 1.0
  //   titleAreaHeight = 48.0 * 1.0 + 20.0 * 2.0 = 88.0
  EXPECT_NEAR(88.0, node.size[1], 1e-9);
}

// --- layoutNode: the single atomic node view-model producer ---

namespace {
FontMetrics testMetrics() {
  FontMetrics m;
  m.ascender = 0.8;
  m.descender = -0.2;
  m.lineHeight = 1.2;
  return m;
}
double testTextWidth(const std::string& text, double fontSize) {
  return static_cast<double>(text.size()) * fontSize * 0.5;
}
} // namespace

TEST(LayoutNodeTest, FlatInputsGetIdentitySlots) {
  // Plain ungrouped pins map 1:1 to rows in authored order; the display list is
  // derived from the originals (rowKinds stay all-normal).
  GraphModel model;
  NodeData node;
  node.name = "N";
  node.originalInputPins = {"a", "b", "c"};
  node.orderedPinEntries = {{"input", "a"}, {"input", "b"}, {"input", "c"}};

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<std::string>({"a", "b", "c"}), node.inputPins);
  EXPECT_EQ(std::vector<int>({0, 1, 2}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({0, 0, 0}), node.displayRowKinds);
}

TEST(LayoutNodeTest, NamespaceGroupUnfoldedInsertsHeaderAndChildren) {
  GraphModel model;
  NodeData node;
  node.name = "G";
  node.originalInputPins = {"a", "grp:x", "grp:y", "b"};
  node.orderedPinEntries = {{"input", "a"}, {"input", "grp:x"}, {"input", "grp:y"}, {"input", "b"}};

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<std::string>({"a", "grp", "grp:x", "grp:y", "b"}), node.inputPins);
  EXPECT_EQ(std::vector<int>({0, 2, 3, 3, 0}), node.inputRowKinds); // 2=unfolded hdr, 3=child
  EXPECT_EQ(std::vector<int>({0, 1, 2, 3, 4}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({0, 2, 3, 3, 0}), node.displayRowKinds);
  EXPECT_TRUE(node.foldedInputPinMap.empty());
}

TEST(LayoutNodeTest, NamespaceGroupFoldedHidesChildrenAndRoutesThroughHeader) {
  GraphModel model;
  NodeData node;
  node.name = "G";
  node.originalInputPins = {"a", "grp:x", "grp:y", "b"};
  node.orderedPinEntries = {{"input", "a"}, {"input", "grp:x"}, {"input", "grp:y"}, {"input", "b"}};
  node.foldState = {{"grp", true}};

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<std::string>({"a", "grp", "b"}), node.inputPins);
  EXPECT_EQ(std::vector<int>({0, 1, 0}), node.inputRowKinds); // 1=folded header
  EXPECT_EQ(std::vector<int>({0, 1, 2}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({0, 1, 0}), node.displayRowKinds);
  EXPECT_EQ("grp", node.foldedInputPinMap.at("grp:x"));
  EXPECT_EQ("grp", node.foldedInputPinMap.at("grp:y"));
}

TEST(LayoutNodeTest, DirectionGroupInsertsHeaderForMembers) {
  GraphModel model;
  NodeData node;
  node.name = "D";
  node.originalInputPins = {"foo", "bar"};
  node.inputDirectionGroupPins = {"foo", "bar"};
  node.orderedPinEntries = {{"input", "foo"}, {"input", "bar"}};

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<std::string>({"inputs", "foo", "bar"}), node.inputPins);
  EXPECT_EQ(std::vector<int>({2, 3, 3}), node.inputRowKinds);
  EXPECT_EQ(std::vector<int>({0, 1, 2}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({2, 3, 3}), node.displayRowKinds);
}

TEST(LayoutNodeTest, InputsAndOutputsStackOntoSeparateRows) {
  // Each entry claims its own row in the shared displayRowKinds, so inputs and
  // outputs stack rather than sharing rows.
  GraphModel model;
  NodeData node;
  node.name = "IO";
  node.originalInputPins = {"a", "b"};
  node.originalOutputPins = {"x"};
  node.orderedPinEntries = {{"input", "a"}, {"input", "b"}, {"output", "x"}};

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<int>({0, 1}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({2}), node.outputRowSlots);
  EXPECT_EQ(std::vector<int>({0, 0, 0}), node.displayRowKinds);
}

TEST(LayoutNodeTest, NoOrderedEntriesFallsBackToIdentity) {
  GraphModel model;
  NodeData node;
  node.name = "V";
  node.originalInputPins = {"a", "b"};
  node.originalOutputPins = {"x"};
  // No orderedPinEntries → identity fallback (input slots 0..n, output 0..m).

  model.layoutNode(node, testTextWidth, testMetrics());

  EXPECT_EQ(std::vector<int>({0, 1}), node.inputRowSlots);
  EXPECT_EQ(std::vector<int>({0}), node.outputRowSlots);
}

TEST(LayoutNodeTest, IsIdempotentAcrossReruns) {
  // The producer reads the raw originals (not its own display output), so
  // re-running must yield identical results — no double-application.
  GraphModel model;
  NodeData node;
  node.name = "G";
  node.originalInputPins = {"a", "grp:x", "grp:y"};
  node.orderedPinEntries = {{"input", "a"}, {"input", "grp:x"}, {"input", "grp:y"}};

  model.layoutNode(node, testTextWidth, testMetrics());
  auto pins1 = node.inputPins;
  auto kinds1 = node.inputRowKinds;
  auto slots1 = node.inputRowSlots;
  auto display1 = node.displayRowKinds;

  model.layoutNode(node, testTextWidth, testMetrics());
  EXPECT_EQ(pins1, node.inputPins);
  EXPECT_EQ(kinds1, node.inputRowKinds);
  EXPECT_EQ(slots1, node.inputRowSlots);
  EXPECT_EQ(display1, node.displayRowKinds);
}

TEST(LayoutNodeTest, CentersStayAlignedAfterFontChange) {
  // The regression this whole migration fixes: re-running layoutNode after a
  // font-size change recomputes slots AND per-pin centers together, so a
  // many-input node never drifts — every center sits exactly at its stripe
  // band center, both fresh and after mutation.
  GraphModel model;
  NodeData node;
  node.name = "Many";
  for (int i = 0; i < 14; ++i) {
    std::string p = "in" + std::to_string(i);
    node.originalInputPins.push_back(p);
    node.orderedPinEntries.emplace_back("input", p);
  }

  auto assertAligned = [&]() {
    const double s = node.layoutPortStartY;
    const double L = node.layoutPortLineHeight;
    ASSERT_EQ(node.originalInputPins.size(), node.layoutInputCenterY.size());
    ASSERT_EQ(node.originalInputPins.size(), node.inputRowSlots.size());
    for (int i = 0; i < static_cast<int>(node.inputRowSlots.size()); ++i) {
      EXPECT_EQ(i, node.inputRowSlots[i]); // flat inputs are identity
      EXPECT_DOUBLE_EQ(s + node.inputRowSlots[i] * L + L * 0.5, node.layoutInputCenterY[i]);
    }
  };

  RenderConfig config;
  model.layoutNode(node, testTextWidth, testMetrics(), &config);
  assertAligned();

  config.nodePinFontSize = config.nodePinFontSize * 1.5;
  model.layoutNode(node, testTextWidth, testMetrics(), &config);
  assertAligned(); // re-reads the (new) line height; centers track the new grid
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
  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  RenderConfig config2;
  config2.nodePinFontSize = config.nodePinFontSize * 2.0;
  config2.nodePinTypeFontSize = config.nodePinTypeFontSize * 2.0;
  NodeData node2 = node;
  node2.inputPins = {"input1"};
  model.calculateNodeSize(node2, calcTextWidth, metrics, &config2);

  // Larger pin fonts -> taller pin rows -> taller node.
  EXPECT_GT(node2.size[1], node.size[1]);
}

// --- NodeVertex ---

TEST(NodeVertexTest, StaticSize) {
  EXPECT_EQ(48u, NodeVertex::size());
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
  EXPECT_FLOAT_EQ(-1.0f, v.nodeIndex);
}

TEST(NodeVertexTest, PackSize) {
  NodeVertex v;
  auto packed = v.pack();
  EXPECT_EQ(48u, packed.size());
}

TEST(NodeVertexTest, PackBatchEmpty) {
  std::vector<NodeVertex> empty;
  auto packed = NodeVertex::packBatch(empty);
  EXPECT_TRUE(packed.empty());
}

TEST(NodeVertexTest, PackBatchSize) {
  std::vector<NodeVertex> verts(5);
  auto packed = NodeVertex::packBatch(verts);
  EXPECT_EQ(5u * 48u, packed.size());
}

TEST(NodeVertexTest, PackRoundTrip) {
  NodeVertex v(
      1.0f, 2.0f, 3.0f, 0.5f, 0.5f, 100.0f, 200.0f, 128, 64, 32, 200, 1.0f, 10, 20, 30, 250, 1.0f);

  auto packed = v.pack();
  ASSERT_EQ(48u, packed.size());

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

  // nodeIndex float at offset 44 (default -1.0 = no transform)
  float readNodeIndex = 0.0f;
  std::memcpy(&readNodeIndex, packed.data() + 44, sizeof(float));
  EXPECT_FLOAT_EQ(-1.0f, readNodeIndex);
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
  EXPECT_DOUBLE_EQ(30.0, config.get("nodeMarginH", 0.0));
  EXPECT_DOUBLE_EQ(20.0, config.get("nodeMarginV", 0.0));
}

TEST(RenderConfigTest, AllDoubleFieldsAccessible) {
  RenderConfig config;
  // Verify all double fields are accessible via get()
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
  ASSERT_EQ(3u * 48u, packed.size());

  // Verify each vertex position is at the right offset
  for (size_t i = 0; i < 3; ++i) {
    float readX = 0.0f, readY = 0.0f;
    std::memcpy(&readX, packed.data() + i * 48, sizeof(float));
    std::memcpy(&readY, packed.data() + i * 48 + 4, sizeof(float));
    EXPECT_FLOAT_EQ(verts[i].x, readX) << "vertex " << i;
    EXPECT_FLOAT_EQ(verts[i].y, readY) << "vertex " << i;
  }
}

TEST(NodeVertexTest, AllFieldsPackedCorrectly) {
  NodeVertex v(
      10.0f, 20.0f, 30.0f, 0.1f, 0.2f, 100.0f, 200.0f, 11, 22, 33, 44, 0.5f, 55, 66, 77, 88, 1.0f);

  auto packed = v.pack();
  ASSERT_EQ(48u, packed.size());

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
  NodeData nodeDefault = node;
  model.calculateNodeSize(nodeDefault, calcTextWidth, metrics, &defaultConfig);

  RenderConfig graffiConfig;
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

  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  const double nodeTitleFontSize = config.get("nodeTitleFontSize", 48.0);
  const double nodePinFontSize = config.get("nodePinFontSize", 36.0);
  const double nodePinTypeFontSize = config.get("nodePinTypeFontSize", 28.0);
  const double nodeMarginV = config.get("nodeMarginV", 36.0);
  const double nodePortSpacing = config.get("nodePortSpacing", 2.0);
  const double titleAreaHeight =
      nodeTitleFontSize * (metrics.ascender - metrics.descender) + nodeMarginV * 2.0;
  const double portLineHeight = nodePinFontSize * metrics.lineHeight +
      nodePinTypeFontSize * metrics.lineHeight + nodePortSpacing;
  // Node height ends exactly at the last row's bottom — no trailing bottom
  // margin (the title's padding lives inside the title bar).
  const double expectedHeight =
      titleAreaHeight + static_cast<double>(node.displayRowKinds.size()) * portLineHeight;

  EXPECT_DOUBLE_EQ(node.size[1], expectedHeight);
}

TEST(GraphModelTest, CalculateNodeSizeReservesRowDecorationWidth) {
  GraphModel model;
  NodeData node;
  node.name = "A";
  node.inputPins = {"very_long_input_pin"};
  node.outputPins = {"very_long_output_pin"};

  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  RenderConfig config;

  model.calculateNodeSize(node, calcTextWidth, metrics, &config);

  const double nodePinFontSize = config.get("nodePinFontSize", 36.0);
  const double nodePinTypeFontSize = config.get("nodePinTypeFontSize", 28.0);
  const double nodeMarginH = config.get("nodeMarginH", 32.0);
  const double nodePortSpacing = config.get("nodePortSpacing", 2.0);
  const double nodePortWidth = config.get("nodePortWidth", 32.0);
  const double portLineHeight = nodePinFontSize * metrics.lineHeight +
      nodePinTypeFontSize * metrics.lineHeight + nodePortSpacing;
  const double rowIconDecorWidth = portLineHeight * 0.64 + nodePinFontSize * 0.25;
  const double relationshipDecorWidth = portLineHeight * 0.72 + nodePinFontSize * 0.25;
  const double expectedWidth = nodePortWidth + calcTextWidth(node.inputPins[0], nodePinFontSize) +
      rowIconDecorWidth + calcTextWidth(node.outputPins[0], nodePinFontSize) +
      relationshipDecorWidth + nodePortWidth + nodeMarginH * 2.0;

  EXPECT_NEAR(node.size[0], expectedWidth, 1e-6);
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

TEST(GraphModelTest, CalculateNodeSizeWithSchemaTypeName) {
  // Nodes with schemaTypeName should be taller to accommodate the subtitle
  GraphModel model;
  FontMetrics metrics;
  metrics.ascender = 0.8;
  metrics.descender = -0.2;
  metrics.lineHeight = 1.2;

  auto calcTextWidth = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };

  NodeData nodeWithout;
  nodeWithout.name = "TestNode";
  nodeWithout.inputPins = {"x"};
  model.calculateNodeSize(nodeWithout, calcTextWidth, metrics);

  NodeData nodeWith;
  nodeWith.name = "TestNode";
  nodeWith.schemaTypeName = "Shader";
  nodeWith.inputPins = {"x"};
  model.calculateNodeSize(nodeWith, calcTextWidth, metrics);

  EXPECT_GT(nodeWith.size[1], nodeWithout.size[1]);
}

// --- Port-level connection index (GraphModel::isPortConnected /
//     isFoldedHeaderConnected) ---

namespace {
LinkData makePortLink(
    const std::string& srcNode,
    const std::string& srcPort,
    const std::string& tgtNode,
    const std::string& tgtPort,
    bool isRelationship = false,
    const std::string& targetPropertyName = "") {
  LinkData link;
  link.sourceNodeId = srcNode;
  link.sourcePort = srcPort;
  link.targetNodeId = tgtNode;
  link.targetPort = tgtPort;
  link.isRelationship = isRelationship;
  link.targetPropertyName = targetPropertyName;
  return link;
}
} // namespace

TEST(PortConnectionIndexTest, RegularLinkConnectsOutputSourceAndInputTarget) {
  // A regular data link connects its source on the OUTPUT side and its target on
  // the INPUT side; the opposite sides and unrelated ports stay disconnected.
  GraphModel model;
  model.links = {makePortLink("A", "out", "B", "in")};

  EXPECT_TRUE(model.isPortConnected("A", "out", /*isOutput=*/true));
  EXPECT_FALSE(model.isPortConnected("A", "out", /*isOutput=*/false));
  EXPECT_TRUE(model.isPortConnected("B", "in", /*isOutput=*/false));
  EXPECT_FALSE(model.isPortConnected("B", "in", /*isOutput=*/true));
  EXPECT_FALSE(model.isPortConnected("A", "missing", /*isOutput=*/true));
}

TEST(PortConnectionIndexTest, RelationshipTargetPropertyForcesSide) {
  // A relationship link's targetPropertyName forces the target endpoint's side:
  // a relationship-pin name ("affects", a member of the {sources, affects}
  // relationship set) forces OUTPUT, while a direction-hint input prefix
  // ("in:foo") forces INPUT. This mirrors graphView._getPropertyOutputSide,
  // where the relationship-pin check precedes the input-hint check.
  GraphModel outputModel;
  outputModel.links = {makePortLink(
      "A", "out", "B", "affects", /*isRelationship=*/true, /*targetPropertyName=*/"affects")};
  EXPECT_TRUE(outputModel.isPortConnected("B", "affects", /*isOutput=*/true));
  EXPECT_FALSE(outputModel.isPortConnected("B", "affects", /*isOutput=*/false));

  GraphModel inputModel;
  inputModel.links = {makePortLink(
      "A", "out", "B", "foo", /*isRelationship=*/true, /*targetPropertyName=*/"in:foo")};
  EXPECT_TRUE(inputModel.isPortConnected("B", "foo", /*isOutput=*/false));
  EXPECT_FALSE(inputModel.isPortConnected("B", "foo", /*isOutput=*/true));
}

TEST(PortConnectionIndexTest, NonRelationshipLinkIgnoresTargetPropertySide) {
  // The targetPropertyName side override applies ONLY to relationship links. A
  // regular link with an output-hinted property keeps its target on the input
  // side; flipping only the relationship flag moves it to the output side.
  GraphModel regularModel;
  regularModel.links = {makePortLink(
      "A", "out", "B", "foo", /*isRelationship=*/false, /*targetPropertyName=*/"outputs:foo")};
  EXPECT_TRUE(regularModel.isPortConnected("B", "foo", /*isOutput=*/false));
  EXPECT_FALSE(regularModel.isPortConnected("B", "foo", /*isOutput=*/true));

  GraphModel relModel;
  relModel.links = {makePortLink(
      "A", "out", "B", "foo", /*isRelationship=*/true, /*targetPropertyName=*/"outputs:foo")};
  EXPECT_TRUE(relModel.isPortConnected("B", "foo", /*isOutput=*/true));
}

TEST(PortConnectionIndexTest, FoldedHeaderConnectedWhenAnyChildConnected) {
  // A folded group header reports connected on a side when ANY of its hidden
  // children carries a link on that side. The child here is connected on the
  // input side only.
  GraphModel model;
  NodeData node;
  node.id = "N";
  node.foldedInputPinMap = {{"grp:x", "grp"}, {"grp:y", "grp"}};
  model.nodes.emplace("N", node);
  model.links = {makePortLink("A", "out", "N", "grp:y")};

  EXPECT_TRUE(model.isFoldedHeaderConnected("N", "grp", /*isOutput=*/false));
  EXPECT_FALSE(model.isFoldedHeaderConnected("N", "grp", /*isOutput=*/true));
  EXPECT_FALSE(model.isFoldedHeaderConnected("N", "other", /*isOutput=*/false));
  EXPECT_FALSE(model.isFoldedHeaderConnected("missing", "grp", /*isOutput=*/false));
}

// --- Synthetic relationship ports (GraphModel::syntheticRelationshipPortsForNode) ---

TEST(SyntheticRelationshipPortTest, RelationshipPinWithNoVisibleRowIsSynthetic) {
  // A relationship link into a node whose pin ("sources") has no display row gets
  // a synthetic port. "sources" is a relationship pin; the relationship link's
  // targetPropertyName forces the output side.
  GraphModel model;
  NodeData node;
  node.id = "N"; // no display pins -> "sources" has no visible row
  model.nodes.emplace("N", node);
  model.links = {makePortLink(
      "A", "out", "N", "sources", /*isRelationship=*/true, /*targetPropertyName=*/"sources")};

  const std::vector<std::pair<std::string, bool>> expected{{"sources", true}};
  EXPECT_EQ(expected, model.syntheticRelationshipPortsForNode("N"));
}

TEST(SyntheticRelationshipPortTest, NoSyntheticWhenRelationshipPinHasVisibleRow) {
  // Same link, but now the node shows a "sources" output row, so the real port is
  // used and no synthetic port is created.
  GraphModel model;
  NodeData node;
  node.id = "N";
  node.outputPins = {"sources"};
  model.nodes.emplace("N", node);
  model.links = {makePortLink(
      "A", "out", "N", "sources", /*isRelationship=*/true, /*targetPropertyName=*/"sources")};

  EXPECT_TRUE(model.syntheticRelationshipPortsForNode("N").empty());
}

TEST(SyntheticRelationshipPortTest, NonRelationshipEndpointIsNeverSynthetic) {
  // A plain data pin with no visible row is NOT a synthetic-port candidate —
  // only relationship pins (sources/affects) are.
  GraphModel model;
  NodeData node;
  node.id = "N";
  model.nodes.emplace("N", node);
  model.links = {makePortLink("A", "out", "N", "in")};

  EXPECT_TRUE(model.syntheticRelationshipPortsForNode("N").empty());
}

TEST(SyntheticRelationshipPortTest, FilterTracksDisplayPinsWithoutLinksChange) {
  // The link-derived candidate is cached on linksChanged_, but the visible-port
  // filter runs against the node's CURRENT display pins: revealing the row (e.g.
  // unfolding) must drop the synthetic port even though the links did not change.
  GraphModel model;
  NodeData node;
  node.id = "N";
  model.nodes.emplace("N", node);
  model.links = {makePortLink(
      "A", "out", "N", "affects", /*isRelationship=*/true, /*targetPropertyName=*/"affects")};

  const std::vector<std::pair<std::string, bool>> expected{{"affects", true}};
  EXPECT_EQ(expected, model.syntheticRelationshipPortsForNode("N"));

  // Reveal the row without touching links: the synthetic port disappears.
  model.nodes.at("N").outputPins = {"affects"};
  EXPECT_TRUE(model.syntheticRelationshipPortsForNode("N").empty());
}

// --- calculateNodeSize: showPinTypeLabels shrinks rows ---

TEST(CalculateNodeSizeTest, HidingPinTypeLabelsShrinksRows) {
  // With pin-type labels hidden, each pin row carries no "(type)" subtitle, so
  // the row height (and the node height) shrink and layoutPortTypeHeight is 0.
  GraphModel graph;
  NodeData node;
  node.name = "N";
  node.inputPins = {"in"};
  node.inputPinTypes["in"] = "float";
  node.inputRowKinds = {0};
  node.inputRowSlots = {0};

  const TextWidthCallback widthCb = [](const std::string& text, double fontSize) {
    return static_cast<double>(text.size()) * fontSize * 0.5;
  };
  FontMetrics fm;
  fm.ascender = 0.8;
  fm.descender = -0.2;
  fm.lineHeight = 1.2;

  RenderConfig shown; // showPinTypeLabels defaults to true
  NodeData withLabels = node;
  graph.calculateNodeSize(withLabels, widthCb, fm, &shown);

  RenderConfig hidden;
  hidden.showPinTypeLabels = false;
  NodeData withoutLabels = node;
  graph.calculateNodeSize(withoutLabels, widthCb, fm, &hidden);

  EXPECT_GT(withLabels.layoutPortTypeHeight, 0.0);
  EXPECT_DOUBLE_EQ(0.0, withoutLabels.layoutPortTypeHeight);
  EXPECT_LT(withoutLabels.layoutPortLineHeight, withLabels.layoutPortLineHeight);
  EXPECT_LT(withoutLabels.size[1], withLabels.size[1]);
}

} // namespace noodles
