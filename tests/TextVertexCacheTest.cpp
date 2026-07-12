// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

//
// Unit tests for TextVertexCache, makeTextLayoutSignature change detection, and
// the pure assembleNodeTextBuffer concat/re-bake. GL-free and FontAtlas-free.

#include <gtest/gtest.h>

#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/RenderConfig.h"
#include "render/NodeRenderManager.h"
#include "render/TextRenderManager.h"
#include "render/TextVertexCache.h"

#include <string>
#include <vector>

namespace noodles {
namespace {

// Non-trivial pin set so all signature fields get exercised.
NodeData makeNode(const std::string& id) {
  NodeData node;
  node.id = id;
  node.name = id;
  node.type = "MyType";
  node.size = {200.0, 100.0};
  node.inputPins = {"in0", "in1"};
  node.outputPins = {"out0"};
  node.inputRowKinds = {0, 0};
  node.outputRowKinds = {0};
  node.inputRowSlots = {0, 1};
  node.outputRowSlots = {0};
  return node;
}

// Fake vertex blob with a sentinel in the depth/nodeIndex slots so re-bake
// is observable.
std::vector<float> fakeBlob(int numChars, float sentinel) {
  std::vector<float> v;
  v.reserve(static_cast<size_t>(numChars) * 36);
  for (int c = 0; c < numChars; ++c) {
    for (int vert = 0; vert < 6; ++vert) {
      v.push_back(1.0f); // x
      v.push_back(2.0f); // y
      v.push_back(sentinel); // z (depth) -> re-baked
      v.push_back(3.0f); // s
      v.push_back(4.0f); // t
      v.push_back(sentinel); // nodeIndex -> re-baked
    }
  }
  return v;
}

} // namespace

// ---------------------------------------------------------------------------
// TextVertexCache
// ---------------------------------------------------------------------------

TEST(TextVertexCacheTest, EmptyCache) {
  TextVertexCache cache;
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("a"));
  EXPECT_TRUE(cache.getVertices("a").empty());
  EXPECT_EQ(0u, cache.getGeneration("a"));
}

TEST(TextVertexCacheTest, IsDirtyWhenMissing) {
  TextVertexCache cache;
  EXPECT_TRUE(cache.isDirty("a", TextLayoutSignature{}));
}

TEST(TextVertexCacheTest, UpdateThenClean) {
  TextVertexCache cache;
  TextLayoutSignature sig;
  sig.name = "a";
  cache.update("a", {1.0f, 2.0f, 3.0f}, sig);
  EXPECT_TRUE(cache.contains("a"));
  EXPECT_EQ(1u, cache.size());
  EXPECT_FALSE(cache.isDirty("a", sig));
  const std::vector<float> expected = {1.0f, 2.0f, 3.0f};
  EXPECT_EQ(expected, cache.getVertices("a"));
}

TEST(TextVertexCacheTest, DirtyWhenEmptyVertices) {
  TextVertexCache cache;
  cache.update("a", {}, TextLayoutSignature{});
  EXPECT_TRUE(cache.isDirty("a", TextLayoutSignature{}));
}

TEST(TextVertexCacheTest, DirtyOnSignatureChange) {
  TextVertexCache cache;
  TextLayoutSignature sigA;
  sigA.name = "a";
  cache.update("a", {1.0f}, sigA);
  TextLayoutSignature sigB = sigA;
  sigB.name = "b";
  EXPECT_TRUE(cache.isDirty("a", sigB));
  EXPECT_FALSE(cache.isDirty("a", sigA));
}

TEST(TextVertexCacheTest, InvalidateNode) {
  TextVertexCache cache;
  TextLayoutSignature sig;
  cache.update("a", {1.0f}, sig);
  EXPECT_FALSE(cache.isDirty("a", sig));
  cache.invalidateNode("a");
  EXPECT_TRUE(cache.isDirty("a", sig));
}

TEST(TextVertexCacheTest, InvalidateAllBumpsGeneration) {
  TextVertexCache cache;
  TextLayoutSignature sig;
  cache.update("a", {1.0f}, sig);
  cache.update("b", {2.0f}, sig);
  cache.invalidateAll();
  EXPECT_TRUE(cache.isDirty("a", sig));
  EXPECT_TRUE(cache.isDirty("b", sig));
  // Re-laying out one node clears only its dirtiness.
  cache.update("a", {1.0f}, sig);
  EXPECT_FALSE(cache.isDirty("a", sig));
  EXPECT_TRUE(cache.isDirty("b", sig));
}

TEST(TextVertexCacheTest, RemoveNode) {
  TextVertexCache cache;
  cache.update("a", {1.0f}, TextLayoutSignature{});
  EXPECT_EQ(1u, cache.size());
  cache.removeNode("a");
  EXPECT_FALSE(cache.contains("a"));
  EXPECT_EQ(0u, cache.size());
}

TEST(TextVertexCacheTest, RetainNodesDropsAbsentIds) {
  TextVertexCache cache;
  cache.update("a", {1.0f}, TextLayoutSignature{});
  cache.update("b", {2.0f}, TextLayoutSignature{});
  cache.update("c", {3.0f}, TextLayoutSignature{});
  cache.retainNodes({"a", "c"});
  EXPECT_TRUE(cache.contains("a"));
  EXPECT_FALSE(cache.contains("b"));
  EXPECT_TRUE(cache.contains("c"));
  EXPECT_EQ(2u, cache.size());
}

TEST(TextVertexCacheTest, Clear) {
  TextVertexCache cache;
  cache.update("a", {1.0f}, TextLayoutSignature{});
  cache.update("b", {2.0f}, TextLayoutSignature{});
  cache.clear();
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("a"));
}

// ---------------------------------------------------------------------------
// makeTextLayoutSignature — change detection (signature-completeness guard)
// ---------------------------------------------------------------------------

TEST(MakeTextLayoutSignatureTest, EqualForIdenticalInputs) {
  RenderConfig config;
  EXPECT_TRUE(
      makeTextLayoutSignature(makeNode("n"), config) ==
      makeTextLayoutSignature(makeNode("n"), config));
}

TEST(MakeTextLayoutSignatureTest, ExcludedFieldsDoNotAffectSignature) {
  RenderConfig config;
  const NodeData base = makeNode("n");
  const TextLayoutSignature sig = makeTextLayoutSignature(base, config);

  // These are the most frequent edits and none of them affect glyph geometry.
  NodeData moved = base;
  moved.position = Vec2d(999.0, 999.0);
  EXPECT_TRUE(makeTextLayoutSignature(moved, config) == sig);

  NodeData raised = base;
  raised.zOrder = 42;
  EXPECT_TRUE(makeTextLayoutSignature(raised, config) == sig);

  NodeData selected = base;
  selected.selected = true;
  EXPECT_TRUE(makeTextLayoutSignature(selected, config) == sig);
}

TEST(MakeTextLayoutSignatureTest, EachLayoutFieldChangesSignature) {
  RenderConfig config;
  const NodeData base = makeNode("n");
  const TextLayoutSignature sig = makeTextLayoutSignature(base, config);
  auto differs = [&](const NodeData& mutated) {
    return makeTextLayoutSignature(mutated, config) != sig;
  };

  {
    NodeData m = base;
    m.name = "other";
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.type = "OtherType";
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.schemaTypeName = "SomeSchema";
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.size = {300.0, 100.0};
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
    m.inputPinTypes["in0"] = "Float";
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.outputPinTypes["out0"] = "Int";
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
    m.relationshipOutputPins.insert("out0");
    EXPECT_TRUE(differs(m));
  }
  {
    NodeData m = base;
    m.titleCollapsed = true;
    EXPECT_TRUE(differs(m));
  }
}

TEST(MakeTextLayoutSignatureTest, ConfigKnobsChangeSignature) {
  const NodeData base = makeNode("n");
  const TextLayoutSignature sig = makeTextLayoutSignature(base, RenderConfig{});
  auto differs = [&](const RenderConfig& config) {
    return makeTextLayoutSignature(base, config) != sig;
  };

  {
    RenderConfig c;
    c.isGraffiStyle = true;
    EXPECT_TRUE(differs(c));
  }
  {
    // Toggling pin-type labels must invalidate cached text (rows shrink + the
    // "(type)" glyphs disappear).
    RenderConfig c;
    c.showPinTypeLabels = false;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodeTitleFontSize += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodePinFontSize += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodePinTypeFontSize += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodeMarginH += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodeMarginV += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodePortSpacing += 1.0;
    EXPECT_TRUE(differs(c));
  }
  {
    RenderConfig c;
    c.nodePortWidth += 1.0;
    EXPECT_TRUE(differs(c));
  }
}

// ---------------------------------------------------------------------------
// TextRenderManager::assembleNodeTextBuffer — concat + depth/nodeIndex re-bake
// ---------------------------------------------------------------------------

TEST(AssembleNodeTextBufferTest, EmptyItems) {
  std::vector<NodeTextAssemblyItem> items;
  std::vector<int> starts;
  std::vector<int> counts;
  const auto buffer = TextRenderManager::assembleNodeTextBuffer(items, starts, counts);
  EXPECT_TRUE(buffer.empty());
  EXPECT_TRUE(starts.empty());
  EXPECT_TRUE(counts.empty());
}

TEST(AssembleNodeTextBufferTest, ConcatenatesInOrderWithSlices) {
  const std::vector<float> blobA = fakeBlob(1, 0.0f); // 36 floats
  const std::vector<float> blobB = fakeBlob(2, 0.0f); // 72 floats
  std::vector<NodeTextAssemblyItem> items = {
      {&blobA, 0.5f, 0.0f},
      {&blobB, 0.7f, 1.0f},
  };
  std::vector<int> starts;
  std::vector<int> counts;
  const auto buffer = TextRenderManager::assembleNodeTextBuffer(items, starts, counts);

  EXPECT_EQ(blobA.size() + blobB.size(), buffer.size());
  const std::vector<int> expectedStarts = {0, static_cast<int>(blobA.size())};
  const std::vector<int> expectedCounts = {1, 2};
  EXPECT_EQ(expectedStarts, starts);
  EXPECT_EQ(expectedCounts, counts);
}

TEST(AssembleNodeTextBufferTest, RebakesDepthAndNodeIndex) {
  const std::vector<float> blob = fakeBlob(2, 99.0f); // sentinel z + nodeIndex
  std::vector<NodeTextAssemblyItem> items = {{&blob, 0.25f, 7.0f}};
  std::vector<int> starts;
  std::vector<int> counts;
  const auto buffer = TextRenderManager::assembleNodeTextBuffer(items, starts, counts);

  ASSERT_EQ(blob.size(), buffer.size());
  for (size_t v = 0; v + 6 <= buffer.size(); v += 6) {
    EXPECT_FLOAT_EQ(1.0f, buffer[v + 0]); // x preserved
    EXPECT_FLOAT_EQ(2.0f, buffer[v + 1]); // y preserved
    EXPECT_FLOAT_EQ(0.25f, buffer[v + 2]); // depth re-baked
    EXPECT_FLOAT_EQ(3.0f, buffer[v + 3]); // s preserved
    EXPECT_FLOAT_EQ(4.0f, buffer[v + 4]); // t preserved
    EXPECT_FLOAT_EQ(7.0f, buffer[v + 5]); // nodeIndex re-baked
  }
}

TEST(AssembleNodeTextBufferTest, SkipsEmptyAndNullBlobs) {
  const std::vector<float> empty;
  const std::vector<float> blob = fakeBlob(1, 0.0f);
  NodeTextAssemblyItem nullItem; // vertices == nullptr
  std::vector<NodeTextAssemblyItem> items = {
      nullItem,
      {&empty, 0.1f, 0.0f},
      {&blob, 0.2f, 1.0f},
  };
  std::vector<int> starts;
  std::vector<int> counts;
  const auto buffer = TextRenderManager::assembleNodeTextBuffer(items, starts, counts);

  EXPECT_EQ(blob.size(), buffer.size());
  const std::vector<int> expectedStarts = {0, 0, 0};
  const std::vector<int> expectedCounts = {0, 0, 1};
  EXPECT_EQ(expectedStarts, starts);
  EXPECT_EQ(expectedCounts, counts);
}

// ---------------------------------------------------------------------------
// TextRenderManager reads GraphModel.nodes (the authoritative render snapshot)
// ---------------------------------------------------------------------------

TEST(TextRenderManagerGraphTest, RendersFromGraphModelNodes) {
  GraphModel graph;
  graph.nodes.emplace("a", makeNode("a"));
  graph.nodes.emplace("b", makeNode("b"));

  NodeTransformFrame frame;
  TextRenderManager mgr;
  mgr.setTransformFrame(&frame);
  // A fresh manager is pending a rebuild, so the Python gate re-syncs on frame 1.
  EXPECT_TRUE(mgr.needsTextRebuild());

  std::vector<float> proj(16, 0.0f);
  RenderConfig config;
  // Not initialized (no GL): renders nothing; exercises reading GraphModel.nodes
  // by reference (the PR4b authoritative-container path) via the shared frame.
  EXPECT_FALSE(
      mgr.renderNodeTextFull(graph.nodes, proj.data(), 1.0f, /*textChanged=*/true, config));
}

// ---------------------------------------------------------------------------
// NodeRenderManager::assembleNodeQuadBuffer — concat + nodeIndex stamp
// ---------------------------------------------------------------------------

namespace {
// One quad vertex with distinct z so we can verify it is preserved (node quads
// bake their own background/stripe/spacer depth — assembly must NOT touch z).
NodeVertex makeQuadVertex(float x, float z) {
  NodeVertex v;
  v.x = x;
  v.z = z;
  v.nodeIndex = -1.0f; // base-frame blobs start unstamped
  return v;
}
} // namespace

TEST(AssembleNodeQuadBufferTest, EmptyItems) {
  std::vector<NodeQuadAssemblyItem> items;
  const auto buffer = NodeRenderManager::assembleNodeQuadBuffer(items);
  EXPECT_TRUE(buffer.empty());
}

TEST(AssembleNodeQuadBufferTest, ConcatenatesInOrderAndStampsNodeIndex) {
  const std::vector<NodeVertex> blobA = {makeQuadVertex(1.0f, 0.001f)};
  const std::vector<NodeVertex> blobB = {
      makeQuadVertex(2.0f, 0.002f), makeQuadVertex(3.0f, 0.002f)};
  std::vector<NodeQuadAssemblyItem> items = {
      {&blobA, 4.0f},
      {&blobB, 9.0f},
  };

  const auto buffer = NodeRenderManager::assembleNodeQuadBuffer(items);
  ASSERT_EQ(3u, buffer.size());

  // blobA stamped with slot 4, z untouched.
  EXPECT_FLOAT_EQ(1.0f, buffer[0].x);
  EXPECT_FLOAT_EQ(0.001f, buffer[0].z);
  EXPECT_FLOAT_EQ(4.0f, buffer[0].nodeIndex);
  // blobB stamped with slot 9, internal z preserved (no flat re-stamp).
  EXPECT_FLOAT_EQ(2.0f, buffer[1].x);
  EXPECT_FLOAT_EQ(0.002f, buffer[1].z);
  EXPECT_FLOAT_EQ(9.0f, buffer[1].nodeIndex);
  EXPECT_FLOAT_EQ(3.0f, buffer[2].x);
  EXPECT_FLOAT_EQ(9.0f, buffer[2].nodeIndex);
}

TEST(AssembleNodeQuadBufferTest, SkipsNullBlobs) {
  const std::vector<NodeVertex> blob = {makeQuadVertex(5.0f, 0.0f)};
  NodeQuadAssemblyItem nullItem; // vertices == nullptr
  std::vector<NodeQuadAssemblyItem> items = {nullItem, {&blob, 2.0f}};

  const auto buffer = NodeRenderManager::assembleNodeQuadBuffer(items);
  ASSERT_EQ(1u, buffer.size());
  EXPECT_FLOAT_EQ(2.0f, buffer[0].nodeIndex);
}

TEST(RenderConfigTest, BackgroundDepthIsZOrderBand) {
  // Background sits at the zOrder band; content half a step in front.
  EXPECT_DOUBLE_EQ(0.0, RenderConfig::backgroundDepth(0));
  EXPECT_DOUBLE_EQ(5 * RenderConfig::kDepthStep, RenderConfig::backgroundDepth(5));
  EXPECT_LT(RenderConfig::backgroundDepth(3), RenderConfig::contentDepth(3));
}

} // namespace noodles
