// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

//
// Unit tests for the C++ icon-render producer. This file covers IconVertexCache
// + makeIconLayoutSignature change detection. GL-free and FontAtlas-free.

#include <gtest/gtest.h>

#include "core/NodeData.h"
#include "core/RenderConfig.h"
#include "render/IconRenderManager.h"
#include "render/IconVertexCache.h"
#include "render/NodeTransformFrame.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {
namespace {

// A node with the fields buildNodeIconQuads / makeIconLayoutSignature read, so
// every signature field gets exercised.
NodeData makeIconNode(const std::string& id) {
  NodeData node;
  node.id = id;
  node.name = id;
  node.titleIconPath = "/icons/" + id + ".png";
  node.inputPins = {"in0", "in1"};
  node.outputPins = {"out0"};
  node.inputRowKinds = {0, 0};
  node.outputRowKinds = {0};
  node.inputRowSlots = {0, 1};
  node.outputRowSlots = {0};
  node.layoutTitleHeight = 40.0;
  node.layoutPortStartY = 40.0;
  node.layoutPortLineHeight = 30.0;
  return node;
}

} // namespace

// ---------------------------------------------------------------------------
// IconVertexCache
// ---------------------------------------------------------------------------

TEST(IconVertexCacheTest, EmptyCache) {
  IconVertexCache cache;
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("a"));
  EXPECT_TRUE(cache.getVertices("a").empty());
  EXPECT_TRUE(cache.getTextureIds("a").empty());
  EXPECT_EQ(0u, cache.getGeneration("a"));
}

TEST(IconVertexCacheTest, IsDirtyWhenMissing) {
  IconVertexCache cache;
  EXPECT_TRUE(cache.isDirty("a", IconLayoutSignature{}));
}

TEST(IconVertexCacheTest, UpdateThenCleanStoresVerticesAndTextures) {
  IconVertexCache cache;
  IconLayoutSignature sig;
  sig.titleIconPath = "a";
  cache.update("a", {1.0f, 2.0f, 3.0f}, {"box", "minus"}, sig);
  EXPECT_TRUE(cache.contains("a"));
  EXPECT_EQ(1u, cache.size());
  EXPECT_FALSE(cache.isDirty("a", sig));
  EXPECT_EQ((std::vector<float>{1.0f, 2.0f, 3.0f}), cache.getVertices("a"));
  EXPECT_EQ((std::vector<std::string>{"box", "minus"}), cache.getTextureIds("a"));
}

TEST(IconVertexCacheTest, DirtyWhenEmptyVertices) {
  IconVertexCache cache;
  cache.update("a", {}, {}, IconLayoutSignature{});
  EXPECT_TRUE(cache.isDirty("a", IconLayoutSignature{}));
}

TEST(IconVertexCacheTest, DirtyOnSignatureChange) {
  IconVertexCache cache;
  IconLayoutSignature sigA;
  sigA.titleIconPath = "a";
  cache.update("a", {1.0f}, {"box"}, sigA);
  IconLayoutSignature sigB = sigA;
  sigB.titleIconPath = "b";
  EXPECT_TRUE(cache.isDirty("a", sigB));
  EXPECT_FALSE(cache.isDirty("a", sigA));
}

TEST(IconVertexCacheTest, InvalidateNode) {
  IconVertexCache cache;
  IconLayoutSignature sig;
  cache.update("a", {1.0f}, {"box"}, sig);
  EXPECT_FALSE(cache.isDirty("a", sig));
  cache.invalidateNode("a");
  EXPECT_TRUE(cache.isDirty("a", sig));
}

TEST(IconVertexCacheTest, InvalidateAllBumpsGeneration) {
  IconVertexCache cache;
  IconLayoutSignature sig;
  cache.update("a", {1.0f}, {"box"}, sig);
  cache.update("b", {2.0f}, {"box"}, sig);
  cache.invalidateAll();
  EXPECT_TRUE(cache.isDirty("a", sig));
  EXPECT_TRUE(cache.isDirty("b", sig));
  // Re-laying out one node clears only its dirtiness.
  cache.update("a", {1.0f}, {"box"}, sig);
  EXPECT_FALSE(cache.isDirty("a", sig));
  EXPECT_TRUE(cache.isDirty("b", sig));
}

TEST(IconVertexCacheTest, RemoveNode) {
  IconVertexCache cache;
  cache.update("a", {1.0f}, {"box"}, IconLayoutSignature{});
  EXPECT_EQ(1u, cache.size());
  cache.removeNode("a");
  EXPECT_FALSE(cache.contains("a"));
  EXPECT_EQ(0u, cache.size());
}

TEST(IconVertexCacheTest, RetainNodesDropsAbsentIds) {
  IconVertexCache cache;
  cache.update("a", {1.0f}, {"box"}, IconLayoutSignature{});
  cache.update("b", {2.0f}, {"box"}, IconLayoutSignature{});
  cache.update("c", {3.0f}, {"box"}, IconLayoutSignature{});
  cache.retainNodes({"a", "c"});
  EXPECT_TRUE(cache.contains("a"));
  EXPECT_FALSE(cache.contains("b"));
  EXPECT_TRUE(cache.contains("c"));
  EXPECT_EQ(2u, cache.size());
}

TEST(IconVertexCacheTest, Clear) {
  IconVertexCache cache;
  cache.update("a", {1.0f}, {"box"}, IconLayoutSignature{});
  cache.update("b", {2.0f}, {"box"}, IconLayoutSignature{});
  cache.clear();
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("a"));
}

// ---------------------------------------------------------------------------
// makeIconLayoutSignature — change detection (signature-completeness guard)
// ---------------------------------------------------------------------------

TEST(MakeIconLayoutSignatureTest, EqualForIdenticalInputs) {
  RenderConfig config;
  EXPECT_TRUE(
      makeIconLayoutSignature(makeIconNode("n"), config) ==
      makeIconLayoutSignature(makeIconNode("n"), config));
}

TEST(MakeIconLayoutSignatureTest, PositionAndZOrderExcluded) {
  RenderConfig config;
  const NodeData base = makeIconNode("n");
  const IconLayoutSignature sig = makeIconLayoutSignature(base, config);

  // Icons bake at the base frame and ride the move offset, so the most frequent
  // edits — moving and re-ordering depth — must NOT invalidate the cached icons.
  NodeData moved = base;
  moved.position = Vec2d(999.0, 999.0);
  EXPECT_TRUE(makeIconLayoutSignature(moved, config) == sig);

  NodeData raised = base;
  raised.zOrder = 42;
  EXPECT_TRUE(makeIconLayoutSignature(raised, config) == sig);
}

TEST(MakeIconLayoutSignatureTest, EachLayoutFieldChangesSignature) {
  RenderConfig config;
  const NodeData base = makeIconNode("n");
  const IconLayoutSignature sig = makeIconLayoutSignature(base, config);
  auto differs = [&](const NodeData& mutated) {
    return makeIconLayoutSignature(mutated, config) != sig;
  };

  {
    NodeData m = base;
    m.titleIconPath = "/icons/other.png";
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.titleCollapsed = true;
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.inputPins = {"in0", "in1", "in2"};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.outputPins = {"out0", "out1"};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.inputRowKinds = {1, 0};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.outputRowKinds = {1};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.inputRowSlots = {2, 3};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.outputRowSlots = {5};
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.relationshipInputPins.insert("in0");
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.relationshipOutputPins.insert("out0");
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.layoutTitleHeight += 1.0;
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.layoutPortStartY += 1.0;
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.layoutPortLineHeight += 1.0;
    EXPECT_TRUE(differs(m));
  }
}

TEST(MakeIconLayoutSignatureTest, ConfigKnobsChangeSignature) {
  const NodeData base = makeIconNode("n");
  const IconLayoutSignature sig = makeIconLayoutSignature(base, RenderConfig{});
  auto differs = [&](const RenderConfig& config) {
    return makeIconLayoutSignature(base, config) != sig;
  };

  {
    RenderConfig c;
    c.nodeTitleFontSize += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodeMarginH += 1.0;
    EXPECT_TRUE(differs(c));
  }
}

// ---------------------------------------------------------------------------
// IconRenderManager::buildNodeIconQuads — placement + minus/relationship pick
// ---------------------------------------------------------------------------

using IconQuad = IconRenderManager::IconQuad;

TEST(BuildNodeIconQuadsTest, TitleIconPlacementAndDefaultFallback) {
  RenderConfig config;
  const double titleFont = config.get("nodeTitleFontSize", 24.0);
  const double marginH = config.get("nodeMarginH", 16.0);

  // Custom title-icon path is used verbatim.
  NodeData node = makeIconNode("n");
  node.titleIconPath = "/icons/custom.png";
  node.titleCollapsed = true; // isolate the title icon (no rows)
  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  ASSERT_EQ(1u, quads.size());
  const IconQuad& title = quads[0];
  EXPECT_EQ("/icons/custom.png", title.textureId);
  // After the title caret (0.5x) + gap (0.15x), vertically centred in the title.
  EXPECT_DOUBLE_EQ(marginH + titleFont * 0.5 + titleFont * 0.15, title.x);
  EXPECT_DOUBLE_EQ(node.layoutTitleHeight * 0.5, title.y);
  EXPECT_DOUBLE_EQ(titleFont * 1.2, title.size);

  // Empty path falls back to the default box key.
  node.titleIconPath = "";
  const auto defaulted = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  ASSERT_EQ(1u, defaulted.size());
  EXPECT_EQ(IconRenderManager::kDefaultTitleIconKey, defaulted[0].textureId);
}

TEST(BuildNodeIconQuadsTest, BaseFrameOffsetsAllQuads) {
  RenderConfig config;
  NodeData node = makeIconNode("n");
  node.titleCollapsed = true;
  const auto atOrigin = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  const auto shifted = IconRenderManager::buildNodeIconQuads(node, config, 100.0, 50.0);
  ASSERT_EQ(1u, atOrigin.size());
  ASSERT_EQ(1u, shifted.size());
  // The base frame translates every quad (icons ride the node's base position).
  EXPECT_DOUBLE_EQ(atOrigin[0].x + 100.0, shifted[0].x);
  EXPECT_DOUBLE_EQ(atOrigin[0].y + 50.0, shifted[0].y);
}

TEST(BuildNodeIconQuadsTest, RowMinusIconsByDefaultDedupedPerSlot) {
  RenderConfig config;
  const double marginH = config.get("nodeMarginH", 16.0);
  // makeIconNode: inputs in0(slot0), in1(slot1); output out0(slot0). Slots {0,1};
  // slot 0 is shared by in0 + out0, so it dedups to a single icon.
  NodeData node = makeIconNode("n");
  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  ASSERT_EQ(3u, quads.size()); // title + 2 row slots

  const double portStartY = node.layoutPortStartY;
  const double lineHeight = node.layoutPortLineHeight;
  for (int slot = 0; slot <= 1; ++slot) {
    const IconQuad& row = quads[1 + slot]; // rows follow the title, in slot order
    EXPECT_EQ(IconRenderManager::kRowMinusIconKey, row.textureId);
    EXPECT_DOUBLE_EQ(marginH, row.x); // left-aligned at the margin
    EXPECT_DOUBLE_EQ(portStartY + slot * lineHeight + lineHeight * 0.5, row.y);
    EXPECT_DOUBLE_EQ(lineHeight * RenderConfig::kRowIconSizeRatio, row.size);
  }
}

TEST(BuildNodeIconQuadsTest, RelationshipRowUsesArrowsIconAndRatio) {
  RenderConfig config;
  NodeData node = makeIconNode("n");
  node.relationshipInputPins.insert("in1"); // slot 1 becomes a relationship row
  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  ASSERT_EQ(3u, quads.size());

  EXPECT_EQ(IconRenderManager::kRowMinusIconKey, quads[1].textureId); // slot 0: minus
  EXPECT_EQ(IconRenderManager::kRelationshipRowIconKey, quads[2].textureId); // slot 1: arrows
  EXPECT_DOUBLE_EQ(
      node.layoutPortLineHeight * RenderConfig::kRelationshipRowIconSizeRatio, quads[2].size);
}

TEST(BuildNodeIconQuadsTest, RelationshipIfEitherSideOfADualRow) {
  RenderConfig config;
  // Single shared slot 0 with an input pin and an output pin; only the OUTPUT
  // side is a relationship pin. The deduped row must still be a relationship row.
  NodeData node;
  node.inputPins = {"shared"};
  node.outputPins = {"shared"};
  node.inputRowKinds = {0};
  node.outputRowKinds = {0};
  node.inputRowSlots = {0};
  node.outputRowSlots = {0};
  node.relationshipOutputPins.insert("shared");
  node.layoutTitleHeight = 40.0;
  node.layoutPortStartY = 40.0;
  node.layoutPortLineHeight = 30.0;

  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  ASSERT_EQ(2u, quads.size()); // title + one deduped row
  EXPECT_EQ(IconRenderManager::kRelationshipRowIconKey, quads[1].textureId);
}

TEST(BuildNodeIconQuadsTest, SkipsFoldCaretRows) {
  RenderConfig config;
  NodeData node = makeIconNode("n");
  node.inputRowKinds = {1, 0}; // in0 is a folded-group caret header -> no icon
  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  // Slot 0: in0 is a caret (skipped) but out0 is a normal row at slot 0, so slot
  // 0 still gets a minus; slot 1 (in1) gets one. Title + 2 rows.
  ASSERT_EQ(3u, quads.size());

  // If BOTH sides of slot 0 were carets, the slot would be dropped entirely.
  NodeData carets = makeIconNode("n");
  carets.inputRowKinds = {2, 0}; // in0 caret
  carets.outputRowKinds = {2}; // out0 caret -> slot 0 has no real row
  const auto fewer = IconRenderManager::buildNodeIconQuads(carets, config, 0.0, 0.0);
  ASSERT_EQ(2u, fewer.size()); // title + only slot 1 (in1)
}

TEST(BuildNodeIconQuadsTest, CollapsedNodeHasOnlyTitleIcon) {
  RenderConfig config;
  NodeData node = makeIconNode("n");
  node.titleCollapsed = true;
  const auto quads = IconRenderManager::buildNodeIconQuads(node, config, 0.0, 0.0);
  EXPECT_EQ(1u, quads.size());
}

TEST(AppendIconQuadVerticesTest, ExpandsToSixVerticesWithUVsAndNodeIndex) {
  IconQuad quad;
  quad.x = 10.0;
  quad.y = 20.0; // vertical centre
  quad.size = 8.0;
  quad.depth = 0.5f;
  std::vector<float> out;
  IconRenderManager::appendIconQuadVertices(quad, /*nodeIndex=*/3.0f, out);

  // 6 vertices x 6 floats (x, y, z, u, v, nodeIndex).
  ASSERT_EQ(36u, out.size());
  // Anchor: x = left edge, y = vertical centre, square `size`.
  const float left = 10.0f, right = 18.0f, top = 16.0f, bottom = 24.0f;
  // First vertex: top-left at uv (0,0).
  EXPECT_FLOAT_EQ(left, out[0]);
  EXPECT_FLOAT_EQ(top, out[1]);
  EXPECT_FLOAT_EQ(0.5f, out[2]); // depth
  EXPECT_FLOAT_EQ(0.0f, out[3]); // u
  EXPECT_FLOAT_EQ(0.0f, out[4]); // v
  EXPECT_FLOAT_EQ(3.0f, out[5]); // nodeIndex
  // Every vertex carries the node index; the quad stays within [left,right] x
  // [top,bottom] with uv in [0,1].
  for (size_t v = 0; v + 6 <= out.size(); v += 6) {
    EXPECT_GE(out[v + 0], left);
    EXPECT_LE(out[v + 0], right);
    EXPECT_GE(out[v + 1], top);
    EXPECT_LE(out[v + 1], bottom);
    EXPECT_FLOAT_EQ(3.0f, out[v + 5]);
  }
}

// ---------------------------------------------------------------------------
// IconRenderManager::assembleIconBuffer — concat + re-bake + per-quad draw items
// ---------------------------------------------------------------------------

namespace {

using NodeIconAssemblyItem = IconRenderManager::NodeIconAssemblyItem;
using IconDrawItem = IconRenderManager::IconDrawItem;

// Build a per-node icon vertex blob of `quadCount` quads using the real producer
// (appendIconQuadVertices) with a placeholder depth/nodeIndex, so the test data is
// independently constructed rather than copied from the assembler's internals.
std::vector<float>
makeNodeIconBlob(int quadCount, float placeholderDepth, float placeholderNodeIndex) {
  std::vector<float> blob;
  for (int q = 0; q < quadCount; ++q) {
    IconQuad quad;
    quad.x = 10.0 * (q + 1);
    quad.y = 5.0 * (q + 1);
    quad.size = 4.0;
    quad.depth = placeholderDepth;
    IconRenderManager::appendIconQuadVertices(quad, placeholderNodeIndex, blob);
  }
  return blob;
}

} // namespace

TEST(AssembleIconBufferTest, EmptyItemsProduceEmptyBufferAndDrawItems) {
  std::vector<IconDrawItem> drawItems;
  const auto buffer = IconRenderManager::assembleIconBuffer({}, drawItems);
  EXPECT_TRUE(buffer.empty());
  EXPECT_TRUE(drawItems.empty());
}

TEST(AssembleIconBufferTest, ConcatenatesBlobsAndEmitsPerQuadDrawItems) {
  // Node A: 2 quads, node B: 1 quad. Independently built blobs + parallel textures.
  const std::vector<float> blobA =
      makeNodeIconBlob(2, /*placeholderDepth=*/0.0f, /*placeholderNodeIndex=*/0.0f);
  const std::vector<float> blobB =
      makeNodeIconBlob(1, /*placeholderDepth=*/0.0f, /*placeholderNodeIndex=*/0.0f);
  const std::vector<std::string> texA{"box", "row-minus"};
  const std::vector<std::string> texB{"relationship-arrows"};

  std::vector<NodeIconAssemblyItem> items;
  items.push_back({&blobA, &texA, /*depth=*/1.0f, /*nodeIndex=*/0.0f, "A"});
  items.push_back({&blobB, &texB, /*depth=*/2.0f, /*nodeIndex=*/1.0f, "B"});

  std::vector<IconDrawItem> drawItems;
  const auto buffer = IconRenderManager::assembleIconBuffer(items, drawItems);

  // Buffer is the concatenation of both blobs.
  ASSERT_EQ(blobA.size() + blobB.size(), buffer.size());

  // One draw item per quad (3 total), in quad order, with textures + node ids.
  ASSERT_EQ(3u, drawItems.size());
  EXPECT_EQ("box", drawItems[0].textureId);
  EXPECT_EQ("row-minus", drawItems[1].textureId);
  EXPECT_EQ("relationship-arrows", drawItems[2].textureId);
  EXPECT_EQ("A", drawItems[0].nodeId);
  EXPECT_EQ("A", drawItems[1].nodeId);
  EXPECT_EQ("B", drawItems[2].nodeId);

  // firstVertex advances by 6 verts per quad; each run is 6 verts.
  EXPECT_EQ(0, drawItems[0].firstVertex);
  EXPECT_EQ(6, drawItems[1].firstVertex);
  EXPECT_EQ(12, drawItems[2].firstVertex);
  for (const auto& di : drawItems) {
    EXPECT_EQ(6, di.vertexCount);
  }
}

TEST(AssembleIconBufferTest, RebakesDepthAndNodeIndexOverEveryVertex) {
  // The cached blob carries a sentinel depth/nodeIndex; assembly must overwrite
  // both with the item's live values so a z-order / slot change needs no relayout.
  const float kSentinel = -7.0f;
  const std::vector<float> blob =
      makeNodeIconBlob(2, /*placeholderDepth=*/kSentinel, /*placeholderNodeIndex=*/kSentinel);
  const std::vector<std::string> tex{"box", "row-minus"};

  std::vector<NodeIconAssemblyItem> items;
  const float kDepth = 0.25f;
  const float kNodeIndex = 9.0f;
  items.push_back({&blob, &tex, kDepth, kNodeIndex, "A"});

  std::vector<IconDrawItem> drawItems;
  const auto buffer = IconRenderManager::assembleIconBuffer(items, drawItems);

  ASSERT_EQ(blob.size(), buffer.size());
  // Stride is 6 floats/vertex; depth at +2, nodeIndex at +5 (see header contract).
  for (size_t v = 0; v + 6 <= buffer.size(); v += 6) {
    EXPECT_FLOAT_EQ(kDepth, buffer[v + 2]);
    EXPECT_FLOAT_EQ(kNodeIndex, buffer[v + 5]);
  }
}

TEST(AssembleIconBufferTest, SkipsNullAndEmptyBlobs) {
  const std::vector<float> blob = makeNodeIconBlob(1, 0.0f, 0.0f);
  const std::vector<std::string> tex{"box"};
  const std::vector<float> emptyBlob;
  const std::vector<std::string> emptyTex;

  std::vector<NodeIconAssemblyItem> items;
  items.push_back({nullptr, &tex, 0.0f, 0.0f, "null"}); // null blob -> skipped
  items.push_back({&emptyBlob, &emptyTex, 0.0f, 1.0f, "empty"}); // empty blob -> skipped
  items.push_back({&blob, &tex, 0.0f, 2.0f, "real"});

  std::vector<IconDrawItem> drawItems;
  const auto buffer = IconRenderManager::assembleIconBuffer(items, drawItems);

  ASSERT_EQ(blob.size(), buffer.size());
  ASSERT_EQ(1u, drawItems.size());
  EXPECT_EQ("box", drawItems[0].textureId);
  EXPECT_EQ("real", drawItems[0].nodeId);
  EXPECT_EQ(0, drawItems[0].firstVertex);
}

// ---------------------------------------------------------------------------
// IconRenderManager::computeVisibleIconDrawItems — transform-aware viewport cull
// ---------------------------------------------------------------------------

namespace {

NodeData makeIconCullNode(double x, double y, double w, double h) {
  NodeData node;
  node.position = Vec2d(x, y);
  node.size = Vec2d(w, h);
  return node;
}

} // namespace

TEST(IconCullTest, CullsByLiveTransformOffsetNotStaleSnapshot) {
  // A node's icons bake at the transform-frame base and ride the live move offset
  // on the GPU, but the GraphModel snapshot (node.position) is NOT re-synced on a
  // drag. Culling on the snapshot would drop a node the user dragged on-screen;
  // the cull must use base + offset, matching where the icons actually draw.
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeIconCullNode(0.0, 0.0, 100.0, 100.0); // snapshot off the viewport

  std::vector<IconDrawItem> drawItems{IconDrawItem{"box", 0, 6, "A"}};

  NodeTransformFrame frame;
  frame.syncFromNodes(nodes); // base = (0, 0)
  frame.translate("A", 300.0, 300.0); // live move: base + offset = (300, 300)

  const double vpMinX = 200.0, vpMinY = 200.0, vpMaxX = 400.0, vpMaxY = 400.0;

  // Stale-snapshot cull (no frame): the node reads as off-screen and is dropped.
  const auto stale = IconRenderManager::computeVisibleIconDrawItems(
      drawItems, nodes, nullptr, vpMinX, vpMinY, vpMaxX, vpMaxY);
  EXPECT_TRUE(stale.empty()) << "snapshot (0,0) is outside the viewport";

  // Transform-aware cull: base + offset = (300, 300) is inside the viewport, so
  // the dragged node's icons are kept.
  const auto visible = IconRenderManager::computeVisibleIconDrawItems(
      drawItems, nodes, &frame, vpMinX, vpMinY, vpMaxX, vpMaxY);
  ASSERT_EQ(1u, visible.size());
  EXPECT_EQ("box", visible[0].textureId);
  EXPECT_EQ("A", visible[0].nodeId);
}

TEST(IconCullTest, CullsNodeDraggedOffViewport) {
  // The reverse: a node whose snapshot is on-screen but whose live drag moved it
  // out must be culled on base + offset (matching the icons, now off-screen).
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeIconCullNode(0.0, 0.0, 100.0, 100.0);

  std::vector<IconDrawItem> drawItems{IconDrawItem{"box", 0, 6, "A"}};

  NodeTransformFrame frame;
  frame.syncFromNodes(nodes);
  frame.translate("A", 5000.0, 5000.0); // dragged far off-screen

  const auto visible = IconRenderManager::computeVisibleIconDrawItems(
      drawItems, nodes, &frame, -10.0, -10.0, 200.0, 200.0);
  EXPECT_TRUE(visible.empty()) << "a node dragged off-screen must be culled";
}

} // namespace noodles
