// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/NodeVertex.h"
#include "render/GraphNodeRenderer.h"
#include "render/LinkGeometry.h"
#include "render/NodeVertexCache.h"
#include "render/TextLayout.h"
#include "render/VertexGenerator.h"

#include <cmath>
#include <unordered_set>

namespace noodles {

// ---------------------------------------------------------------------------
// VertexGenerator -- hsvToRgb
// ---------------------------------------------------------------------------

TEST(HsvToRgbTest, PureRed) {
  auto c = VertexGenerator::hsvToRgb(0.0f, 1.0f, 1.0f);
  EXPECT_NEAR(1.0f, c[0], 1e-5f); // R
  EXPECT_NEAR(0.0f, c[1], 1e-5f); // G
  EXPECT_NEAR(0.0f, c[2], 1e-5f); // B
  EXPECT_FLOAT_EQ(1.0f, c[3]); // A
}

TEST(HsvToRgbTest, PureGreen) {
  auto c = VertexGenerator::hsvToRgb(120.0f, 1.0f, 1.0f);
  EXPECT_NEAR(0.0f, c[0], 1e-5f);
  EXPECT_NEAR(1.0f, c[1], 1e-5f);
  EXPECT_NEAR(0.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, PureBlue) {
  auto c = VertexGenerator::hsvToRgb(240.0f, 1.0f, 1.0f);
  EXPECT_NEAR(0.0f, c[0], 1e-5f);
  EXPECT_NEAR(0.0f, c[1], 1e-5f);
  EXPECT_NEAR(1.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, Yellow) {
  auto c = VertexGenerator::hsvToRgb(60.0f, 1.0f, 1.0f);
  EXPECT_NEAR(1.0f, c[0], 1e-5f);
  EXPECT_NEAR(1.0f, c[1], 1e-5f);
  EXPECT_NEAR(0.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, Cyan) {
  auto c = VertexGenerator::hsvToRgb(180.0f, 1.0f, 1.0f);
  EXPECT_NEAR(0.0f, c[0], 1e-5f);
  EXPECT_NEAR(1.0f, c[1], 1e-5f);
  EXPECT_NEAR(1.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, Magenta) {
  auto c = VertexGenerator::hsvToRgb(300.0f, 1.0f, 1.0f);
  EXPECT_NEAR(1.0f, c[0], 1e-5f);
  EXPECT_NEAR(0.0f, c[1], 1e-5f);
  EXPECT_NEAR(1.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, White) {
  auto c = VertexGenerator::hsvToRgb(0.0f, 0.0f, 1.0f);
  EXPECT_NEAR(1.0f, c[0], 1e-5f);
  EXPECT_NEAR(1.0f, c[1], 1e-5f);
  EXPECT_NEAR(1.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, Black) {
  auto c = VertexGenerator::hsvToRgb(0.0f, 0.0f, 0.0f);
  EXPECT_NEAR(0.0f, c[0], 1e-5f);
  EXPECT_NEAR(0.0f, c[1], 1e-5f);
  EXPECT_NEAR(0.0f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, HalfSaturation) {
  auto c = VertexGenerator::hsvToRgb(0.0f, 0.5f, 1.0f);
  EXPECT_NEAR(1.0f, c[0], 1e-5f);
  EXPECT_NEAR(0.5f, c[1], 1e-5f);
  EXPECT_NEAR(0.5f, c[2], 1e-5f);
}

TEST(HsvToRgbTest, CustomAlpha) {
  auto c = VertexGenerator::hsvToRgb(0.0f, 1.0f, 1.0f, 0.5f);
  EXPECT_FLOAT_EQ(0.5f, c[3]);
}

TEST(HsvToRgbTest, DegreesInputProducesFullColorRange) {
  // With correct degree-range input [0,360), all hue sectors are reachable.
  bool hasGreen = false;
  bool hasBlue = false;
  for (int i = 0; i < 360; ++i) {
    auto hue = static_cast<float>(i);
    auto c = VertexGenerator::hsvToRgb(hue, 1.0f, 1.0f);
    if (c[1] > 0.5f) {
      hasGreen = true;
    }
    if (c[2] > 0.5f) {
      hasBlue = true;
    }
  }
  EXPECT_TRUE(hasGreen) << "Green channel should be reachable with degree input";
  EXPECT_TRUE(hasBlue) << "Blue channel should be reachable with degree input";
}

// ---------------------------------------------------------------------------
// VertexGenerator -- generateDefaultNodeVertices
// ---------------------------------------------------------------------------

TEST(VertexGeneratorTest, DefaultNodeVertexCount) {
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, 1.0f, 0.0f);
  EXPECT_EQ(12u, verts.size());
}

TEST(VertexGeneratorTest, DefaultNodeVertexPositionBounds) {
  Vec2d pos(100, 200);
  Vec2d size(300, 150);
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts =
      VertexGenerator::generateDefaultNodeVertices(pos, size, 0.5f, 40.0f, colors, 1.0f, 0.0f);

  for (const auto& v : verts) {
    EXPECT_GE(v.x, 100.0f);
    EXPECT_LE(v.x, 400.0f);
    EXPECT_GE(v.y, 200.0f);
    EXPECT_LE(v.y, 350.0f);
    EXPECT_FLOAT_EQ(0.5f, v.z);
  }
}

TEST(VertexGeneratorTest, DefaultNodeColorAssignment) {
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, 1.0f, 0.0f);

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(248, verts[i].a) << "bg vertex " << i << " alpha";
  }
  for (int i = 6; i < 12; ++i) {
    EXPECT_EQ(255, verts[i].a) << "title vertex " << i << " alpha";
  }
}

TEST(VertexGeneratorTest, SelectedFlagPropagated) {
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, 1.0f, 1.0f);
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(1.0f, v.selected);
  }
}

// Helper: extract the grey value (r channel) of the first vertex of row i's stripe.
// Layout: 12 base (bg+title) + i * 12 (6 stripe + 6 spacer per row).
static uint8_t getRowGrey(const std::vector<NodeVertex>& verts, int row) {
  return verts[12 + row * 12].r;
}
static uint8_t getRowSelectedGrey(const std::vector<NodeVertex>& verts, int row) {
  return verts[12 + row * 12].sr;
}

TEST(VertexGeneratorTest, RowColorAlternation_NoGroups) {
  // With empty rowKinds, all rows are kind 0 (normal) and should alternate 60/55.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 4, 20.0f, 35.0f, {});

  EXPECT_EQ(60, getRowGrey(verts, 0)); // even
  EXPECT_EQ(55, getRowGrey(verts, 1)); // odd
  EXPECT_EQ(60, getRowGrey(verts, 2)); // even
  EXPECT_EQ(55, getRowGrey(verts, 3)); // odd
}

TEST(VertexGeneratorTest, RowColorAlternation_NormalThenUnfoldedGroup) {
  // rowKinds = [0, 2, 3, 3, 0]
  // Normal rows should alternate independently of group/child rows.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 2, 3, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 5, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(60, getRowGrey(verts, 0)); // kind 0, nrc=0 → 60
  EXPECT_EQ(60, getRowGrey(verts, 1)); // kind 2, fixed → 60
  EXPECT_EQ(38, getRowGrey(verts, 2)); // kind 3, fixed → 38
  EXPECT_EQ(38, getRowGrey(verts, 3)); // kind 3, fixed → 38
  EXPECT_EQ(55, getRowGrey(verts, 4)); // kind 0, nrc=1 → 55
}

TEST(VertexGeneratorTest, RowColorAlternation_GroupDoesNotShiftNormalParity) {
  // rowKinds = [0, 0, 2, 3, 3, 0]
  // The two normal rows at the start alternate 60/55.
  // After the group, the next normal row should continue the count (nrc=2 → 60).
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 0, 2, 3, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 6, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(60, getRowGrey(verts, 0)); // kind 0, nrc=0 → 60
  EXPECT_EQ(55, getRowGrey(verts, 1)); // kind 0, nrc=1 → 55
  EXPECT_EQ(60, getRowGrey(verts, 2)); // kind 2, fixed → 60
  EXPECT_EQ(38, getRowGrey(verts, 3)); // kind 3, fixed → 38
  EXPECT_EQ(38, getRowGrey(verts, 4)); // kind 3, fixed → 38
  EXPECT_EQ(60, getRowGrey(verts, 5)); // kind 0, nrc=2 → 60
}

TEST(VertexGeneratorTest, RowColorAlternation_SelectedColors) {
  // Verify selected colors follow the same pattern.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 2, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 4, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(78, getRowSelectedGrey(verts, 0)); // kind 0, nrc=0 → 78
  EXPECT_EQ(78, getRowSelectedGrey(verts, 1)); // kind 2, fixed → 78
  EXPECT_EQ(52, getRowSelectedGrey(verts, 2)); // kind 3, fixed → 52
  EXPECT_EQ(72, getRowSelectedGrey(verts, 3)); // kind 0, nrc=1 → 72
}

TEST(VertexGeneratorTest, RowColorAlternation_ParentAlwaysLighterThanChildren) {
  // In ANY configuration, header rows (kind 2) must be lighter than child rows (kind 3).
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 0, 0, 2, 3, 3, 0, 2, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 400), 0.0f, 30.0f, colors, 0.0f, 0.0f, 10, 20.0f, 35.0f, rowKinds);

  // Check every header is lighter than every child
  for (int i = 0; i < 10; ++i) {
    if (rowKinds[i] == 2) {
      for (int j = 0; j < 10; ++j) {
        if (rowKinds[j] == 3) {
          EXPECT_GT(getRowGrey(verts, i), getRowGrey(verts, j))
              << "Header row " << i << " should be lighter than child row " << j;
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// VertexGenerator -- generateGraffiNodeVertices
// ---------------------------------------------------------------------------

TEST(GraffiVertexTest, NoPortsUnselected) {
  GraffiNodeColors colors{40, 40, 40, 200, 80, 80, 80, 180, 60, 60, 60, 100, 100, 100};
  auto verts = VertexGenerator::generateGraffiNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, {}, {}, 6.0f, 1.0f, 0.0f);
  EXPECT_EQ(12u, verts.size());
}

TEST(GraffiVertexTest, NoPortsSelected) {
  GraffiNodeColors colors{40, 40, 40, 200, 80, 80, 80, 180, 60, 60, 60, 100, 100, 100};
  auto verts = VertexGenerator::generateGraffiNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, {}, {}, 6.0f, 1.0f, 1.0f);
  EXPECT_EQ(18u, verts.size());
}

TEST(GraffiVertexTest, WithPorts) {
  std::vector<PortInfo> inputs = {
      {Vec2d(10, 50), 255, 0, 0, 255},
      {Vec2d(10, 70), 0, 255, 0, 255},
  };
  std::vector<PortInfo> outputs = {
      {Vec2d(190, 50), 0, 0, 255, 255},
  };
  GraffiNodeColors colors{40, 40, 40, 200, 80, 80, 80, 180, 60, 60, 60, 100, 100, 100};
  auto verts = VertexGenerator::generateGraffiNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, inputs, outputs, 6.0f, 1.0f, 0.0f);
  EXPECT_EQ(30u, verts.size());
}

TEST(GraphNodeRendererTest, PortPositionsUseAuthoredRowSlots) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  FontAtlas fontAtlas;

  NodeData node;
  node.position = Vec2d(10.0, 20.0);
  node.size = Vec2d(240.0, 320.0);
  node.inputPins = {"inputs", "foo", "middle", "bar"};
  node.inputRowKinds = {2, 3, 0, 3};
  node.inputRowSlots = {0, 1, 4, 5};
  node.outputPins = {"outputs", "left", "right"};
  node.outputRowKinds = {2, 3, 3};
  node.outputRowSlots = {2, 3, 6};

  const double titleHeight =
      renderer.getTitleFontSize() * (fontAtlas.ascender() - fontAtlas.descender()) +
      renderer.getPortMarginV() * 2.0;
  const double portNameHeight = renderer.getPortFontSize() * fontAtlas.lineHeight();
  const double portTypeHeight = renderer.getPortTypeFontSize() * fontAtlas.lineHeight();
  const double portLineHeight = portNameHeight + portTypeHeight + renderer.getPortSpacing();
  const double portStartY = node.position[1] + titleHeight + renderer.getPortMarginV();

  const Vec2d inputFoo = renderer.getPortPosition(node, "foo", false, fontAtlas);
  const Vec2d outputLeft = renderer.getPortPosition(node, "left", true, fontAtlas);
  const Vec2d middle = renderer.getPortPosition(node, "middle", false, fontAtlas);
  const Vec2d inputBar = renderer.getPortPosition(node, "bar", false, fontAtlas);

  EXPECT_DOUBLE_EQ(inputFoo[1], portStartY + 1.0 * portLineHeight + portNameHeight * 0.5);
  EXPECT_DOUBLE_EQ(outputLeft[1], portStartY + 3.0 * portLineHeight + portNameHeight * 0.5);
  EXPECT_DOUBLE_EQ(middle[1], portStartY + 4.0 * portLineHeight + portNameHeight * 0.5);
  EXPECT_DOUBLE_EQ(inputBar[1], portStartY + 5.0 * portLineHeight + portNameHeight * 0.5);
  EXPECT_LT(inputFoo[1], outputLeft[1]);
  EXPECT_LT(outputLeft[1], middle[1]);
  EXPECT_LT(middle[1], inputBar[1]);
}

// ---------------------------------------------------------------------------
// LinkGeometry
// ---------------------------------------------------------------------------

TEST(LinkGeometryTest, FindLinkUnderCursorNoLinks) {
  std::vector<std::pair<Vec2d, Vec2d>> endpoints;
  EXPECT_EQ(-1, LinkGeometry::findLinkUnderCursor(Vec2d(0, 0), endpoints, 10.0));
}

TEST(LinkGeometryTest, FindLinkUnderCursorHit) {
  std::vector<std::pair<Vec2d, Vec2d>> endpoints = {
      {Vec2d(0, 0), Vec2d(100, 0)},
  };
  // Point on the line
  EXPECT_EQ(0, LinkGeometry::findLinkUnderCursor(Vec2d(50, 0), endpoints, 5.0));
  // Point near the line
  EXPECT_EQ(0, LinkGeometry::findLinkUnderCursor(Vec2d(50, 3), endpoints, 5.0));
}

TEST(LinkGeometryTest, FindLinkUnderCursorMiss) {
  std::vector<std::pair<Vec2d, Vec2d>> endpoints = {
      {Vec2d(0, 0), Vec2d(100, 0)},
  };
  // Point far from the line
  EXPECT_EQ(-1, LinkGeometry::findLinkUnderCursor(Vec2d(50, 20), endpoints, 5.0));
}

TEST(LinkGeometryTest, FindLinkUnderCursorClosest) {
  std::vector<std::pair<Vec2d, Vec2d>> endpoints = {
      {Vec2d(0, 0), Vec2d(100, 0)},
      {Vec2d(0, 5), Vec2d(100, 5)},
  };
  // Closer to second link
  EXPECT_EQ(1, LinkGeometry::findLinkUnderCursor(Vec2d(50, 4), endpoints, 5.0));
  // Closer to first link
  EXPECT_EQ(0, LinkGeometry::findLinkUnderCursor(Vec2d(50, 1), endpoints, 5.0));
}

TEST(LinkGeometryTest, LinkIntersectsBoundsEndpointInside) {
  Range2d bounds(Vec2d(10, 10), Vec2d(50, 50));
  EXPECT_TRUE(LinkGeometry::linkIntersectsBounds(Vec2d(20, 20), Vec2d(100, 100), bounds));
}

TEST(LinkGeometryTest, LinkIntersectsBoundsCrossing) {
  Range2d bounds(Vec2d(10, 10), Vec2d(50, 50));
  // Line crosses through the rectangle but neither endpoint is inside
  EXPECT_TRUE(LinkGeometry::linkIntersectsBounds(Vec2d(0, 30), Vec2d(60, 30), bounds));
}

TEST(LinkGeometryTest, LinkIntersectsBoundsMiss) {
  Range2d bounds(Vec2d(10, 10), Vec2d(50, 50));
  EXPECT_FALSE(LinkGeometry::linkIntersectsBounds(Vec2d(0, 0), Vec2d(5, 5), bounds));
}

TEST(LinkGeometryTest, FindLinksInBounds) {
  std::vector<std::pair<Vec2d, Vec2d>> endpoints = {
      {Vec2d(0, 0), Vec2d(100, 0)}, // crosses bounds
      {Vec2d(200, 200), Vec2d(300, 300)}, // outside
      {Vec2d(30, 30), Vec2d(40, 40)}, // inside bounds
  };
  Range2d bounds(Vec2d(10, -10), Vec2d(50, 50));
  auto result = LinkGeometry::findLinksInBounds(endpoints, bounds);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0, result[0]);
  EXPECT_EQ(2, result[1]);
}

TEST(LinkGeometryTest, CalculateLODHighSegments) {
  std::vector<std::pair<int, int>> thresholds = {
      {200, 160},
      {100, 80},
      {50, 40},
      {20, 20},
  };
  // manhattanLength=1000, diffX=1 => numSegments=1000 > 200 => LOD 160
  EXPECT_EQ(160, LinkGeometry::calculateLOD(1000.0, 1.0, thresholds));
}

TEST(LinkGeometryTest, CalculateLODLowSegments) {
  std::vector<std::pair<int, int>> thresholds = {
      {200, 160},
      {100, 80},
      {50, 40},
      {20, 20},
  };
  // manhattanLength=10, diffX=1 => numSegments=10, below all thresholds => minimum 5
  EXPECT_EQ(5, LinkGeometry::calculateLOD(10.0, 1.0, thresholds));
}

TEST(LinkGeometryTest, CalculateLODZeroDiffX) {
  std::vector<std::pair<int, int>> thresholds = {{200, 160}};
  EXPECT_EQ(160, LinkGeometry::calculateLOD(100.0, 0.0, thresholds));
}

TEST(LinkGeometryTest, CalculateLODEmptyThresholds) {
  EXPECT_EQ(160, LinkGeometry::calculateLOD(100.0, 0.0, {}));
}

TEST(LinkGeometryTest, GenerateReferenceCurveMinSamples) {
  EXPECT_TRUE(LinkGeometry::generateReferenceCurve(0).empty());
  EXPECT_TRUE(LinkGeometry::generateReferenceCurve(1).empty());
}

TEST(LinkGeometryTest, GenerateReferenceCurveVertexCount) {
  auto curve = LinkGeometry::generateReferenceCurve(10);
  // 7 floats per vertex, 2 vertices per sample
  EXPECT_EQ(10u * 2u * 7u, curve.size());
}

TEST(LinkGeometryTest, GenerateReferenceCurveEndpoints) {
  auto curve = LinkGeometry::generateReferenceCurve(10);
  // First vertex position should be at t=0 → (0, 0)
  EXPECT_NEAR(0.0f, curve[0], 1e-5f); // pos.x
  EXPECT_NEAR(0.0f, curve[1], 1e-5f); // pos.y

  // Last pair of vertices should be at t=1 → (1.0, 1.0)
  size_t lastIdx = curve.size() - 7; // second-to-last vertex of last sample
  EXPECT_NEAR(1.0f, curve[lastIdx], 1e-5f); // pos.x
  EXPECT_NEAR(1.0f, curve[lastIdx + 1], 1e-3f); // pos.y (cos^2 at t=1)
}

TEST(LinkGeometryTest, GenerateReferenceCurveDirectionFlags) {
  auto curve = LinkGeometry::generateReferenceCurve(5);
  // Pairs of vertices alternate dir: -1.0 then +1.0
  for (size_t i = 0; i < 5; ++i) {
    size_t base = i * 2 * 7;
    EXPECT_FLOAT_EQ(-1.0f, curve[base + 6]); // first vertex dir
    EXPECT_FLOAT_EQ(1.0f, curve[base + 7 + 6]); // second vertex dir
  }
}

// ---------------------------------------------------------------------------
// NodeVertexCache
// ---------------------------------------------------------------------------

TEST(NodeVertexCacheTest, EmptyCache) {
  NodeVertexCache cache;
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("node1"));
  EXPECT_TRUE(cache.getVertices("node1").empty());
  EXPECT_EQ(0u, cache.getGeneration("node1"));
}

TEST(NodeVertexCacheTest, IsDirtyWhenMissing) {
  NodeVertexCache cache;
  EXPECT_TRUE(cache.isDirty("node1", Vec2d(0, 0), Vec2d(200, 100)));
}

TEST(NodeVertexCacheTest, UpdateAndRetrieve) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  verts[0].x = 42.0f;

  cache.update("node1", verts, Vec2d(10, 20), Vec2d(200, 100), false);

  EXPECT_TRUE(cache.contains("node1"));
  EXPECT_EQ(1u, cache.size());
  EXPECT_FALSE(cache.isDirty("node1", Vec2d(10, 20), Vec2d(200, 100)));
  EXPECT_EQ(6u, cache.getVertices("node1").size());
  EXPECT_FLOAT_EQ(42.0f, cache.getVertices("node1")[0].x);
}

TEST(NodeVertexCacheTest, DirtyOnPositionChange) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(10, 20), Vec2d(200, 100), false);

  EXPECT_TRUE(cache.isDirty("node1", Vec2d(50, 20), Vec2d(200, 100)));
}

TEST(NodeVertexCacheTest, DirtyOnSizeChange) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(10, 20), Vec2d(200, 100), false);

  EXPECT_TRUE(cache.isDirty("node1", Vec2d(10, 20), Vec2d(300, 100)));
}

TEST(NodeVertexCacheTest, InvalidateNode) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(200, 100), false);
  EXPECT_FALSE(cache.isDirty("node1", Vec2d(0, 0), Vec2d(200, 100)));

  cache.invalidateNode("node1");
  EXPECT_TRUE(cache.isDirty("node1", Vec2d(0, 0), Vec2d(200, 100)));
}

TEST(NodeVertexCacheTest, InvalidateAll) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("a", verts, Vec2d(0, 0), Vec2d(100, 100), false);
  cache.update("b", verts, Vec2d(0, 0), Vec2d(100, 100), false);

  cache.invalidateAll();

  EXPECT_TRUE(cache.isDirty("a", Vec2d(0, 0), Vec2d(100, 100)));
  EXPECT_TRUE(cache.isDirty("b", Vec2d(0, 0), Vec2d(100, 100)));
}

TEST(NodeVertexCacheTest, RemoveNode) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(200, 100), false);
  EXPECT_EQ(1u, cache.size());

  cache.removeNode("node1");
  EXPECT_EQ(0u, cache.size());
  EXPECT_FALSE(cache.contains("node1"));
}

TEST(NodeVertexCacheTest, Clear) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("a", verts, Vec2d(0, 0), Vec2d(100, 100), false);
  cache.update("b", verts, Vec2d(0, 0), Vec2d(100, 100), false);

  cache.clear();
  EXPECT_EQ(0u, cache.size());
}

TEST(NodeVertexCacheTest, UpdateSelectedFlagFlips) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  for (auto& v : verts) {
    v.selected = 0.0f;
    v.innerStroke = 0.0f;
  }
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(200, 100), false);

  bool changed = cache.updateSelectedFlag("node1", true, 2.0f);
  EXPECT_TRUE(changed);

  const auto& updated = cache.getVertices("node1");
  for (const auto& v : updated) {
    EXPECT_FLOAT_EQ(1.0f, v.selected);
    EXPECT_FLOAT_EQ(2.0f, v.innerStroke);
  }
}

TEST(NodeVertexCacheTest, UpdateSelectedFlagNoChange) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(200, 100), false);

  // Already unselected, no change
  bool changed = cache.updateSelectedFlag("node1", false, 0.0f);
  EXPECT_FALSE(changed);
}

TEST(NodeVertexCacheTest, UpdateSelectedFlagMissingNode) {
  NodeVertexCache cache;
  EXPECT_FALSE(cache.updateSelectedFlag("missing", true, 1.0f));
}

TEST(NodeVertexCacheTest, GenerationIncrementsOnInvalidateAll) {
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(100, 100), false);
  uint32_t gen1 = cache.getGeneration("node1");

  cache.invalidateAll();
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(100, 100), false);
  uint32_t gen2 = cache.getGeneration("node1");

  EXPECT_GT(gen2, gen1);
}

// ---------------------------------------------------------------------------
// TextLayout
// ---------------------------------------------------------------------------

class TextLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up a minimal glyph map for ASCII letters
    // Using simple advance values for predictable width calculations
    for (int c = 32; c < 127; ++c) {
      GlyphMetrics gm;
      gm.unicode = c;
      gm.advance = 0.5; // uniform advance
      gm.planeBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.4, 0.8));
      gm.atlasBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.1, 0.1));
      glyphMap_[c] = gm;
    }
  }

  std::unordered_map<int, GlyphMetrics> glyphMap_;
};

TEST_F(TextLayoutTest, EmptyString) {
  double w = TextLayout::CalculateTextWidth("", 16.0, glyphMap_);
  EXPECT_DOUBLE_EQ(0.0, w);
}

TEST_F(TextLayoutTest, EmptyGlyphMap) {
  std::unordered_map<int, GlyphMetrics> empty;
  double w = TextLayout::CalculateTextWidth("hello", 16.0, empty);
  // Fallback: len * fontSize * 0.5
  EXPECT_DOUBLE_EQ(5.0 * 16.0 * 0.5, w);
}

TEST_F(TextLayoutTest, CalculateTextWidth) {
  double w = TextLayout::CalculateTextWidth("Hello", 16.0, glyphMap_);
  // 5 chars * 0.5 advance * 16.0 fontSize = 40.0
  EXPECT_DOUBLE_EQ(40.0, w);
}

TEST_F(TextLayoutTest, CalculateTextWidthBatch) {
  std::vector<std::string> texts = {"Hi", "Hello", "A"};
  auto widths = TextLayout::CalculateTextWidthBatch(texts, 10.0, glyphMap_);
  ASSERT_EQ(3u, widths.size());
  EXPECT_DOUBLE_EQ(2.0 * 0.5 * 10.0, widths[0]);
  EXPECT_DOUBLE_EQ(5.0 * 0.5 * 10.0, widths[1]);
  EXPECT_DOUBLE_EQ(1.0 * 0.5 * 10.0, widths[2]);
}

TEST_F(TextLayoutTest, GenerateTextVerticesEmpty) {
  std::vector<float> vertexData;
  auto result =
      TextLayout::GenerateTextVertices("", 0.0, 0.0, 0.0, 16.0, glyphMap_, 0.8, 0.0, vertexData);
  EXPECT_TRUE(vertexData.empty());
  EXPECT_EQ(0, result.charCount);
}

TEST_F(TextLayoutTest, GenerateTextVerticesCount) {
  std::vector<float> vertexData;
  auto result =
      TextLayout::GenerateTextVertices("AB", 0.0, 0.0, 0.5, 16.0, glyphMap_, 0.8, 3.0, vertexData);
  // 2 chars * 6 vertices * 6 floats = 72
  EXPECT_EQ(72u, vertexData.size());
  EXPECT_EQ(2, result.charCount);
}

TEST_F(TextLayoutTest, GenerateTextVerticesDepthAndNodeIndex) {
  std::vector<float> vertexData;
  TextLayout::GenerateTextVertices("X", 0.0, 0.0, 0.5, 16.0, glyphMap_, 0.8, 7.0, vertexData);
  // Each vertex: x, y, z, s, t, nodeIndex
  // Check first vertex
  ASSERT_GE(vertexData.size(), 6u);
  EXPECT_FLOAT_EQ(0.5f, vertexData[2]); // z = depth
  EXPECT_FLOAT_EQ(7.0f, vertexData[5]); // nodeIndex
}

TEST_F(TextLayoutTest, GenerateTextVerticesCursorAdvances) {
  std::vector<float> vertexData;
  auto result = TextLayout::GenerateTextVertices(
      "ABC", 10.0, 0.0, 0.0, 16.0, glyphMap_, 0.8, 0.0, vertexData);
  // cursorX should advance: 10.0 + 3 * 0.5 * 16.0 = 34.0
  EXPECT_DOUBLE_EQ(34.0, result.cursorX);
}

TEST_F(TextLayoutTest, GenerateTextVerticesAppendsToExisting) {
  std::vector<float> vertexData = {1.0f, 2.0f, 3.0f};
  TextLayout::GenerateTextVertices("A", 0.0, 0.0, 0.0, 16.0, glyphMap_, 0.8, 0.0, vertexData);
  // 3 existing + 1 char * 6 verts * 6 floats = 39
  EXPECT_EQ(39u, vertexData.size());
  // Original data preserved
  EXPECT_FLOAT_EQ(1.0f, vertexData[0]);
}

TEST_F(TextLayoutTest, Utf8TwoByteCharacter) {
  GlyphMetrics gm;
  gm.unicode = 0x00E9;
  gm.advance = 0.6;
  gm.planeBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.4, 0.8));
  gm.atlasBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.1, 0.1));
  glyphMap_[0x00E9] = gm;

  // "café" = 4 logical chars, 5 bytes
  double w = TextLayout::CalculateTextWidth("caf\xC3\xA9", 10.0, glyphMap_);
  // c(0.5) + a(0.5) + f(0.5) + é(0.6) = 2.1, * 10.0 = 21.0
  EXPECT_DOUBLE_EQ(21.0, w);
}

TEST_F(TextLayoutTest, Utf8ThreeByteCharacter) {
  // U+4E16 (世, 3-byte UTF-8: 0xE4 0xB8 0x96)
  GlyphMetrics gm;
  gm.unicode = 0x4E16;
  gm.advance = 1.0;
  gm.planeBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.9, 0.9));
  gm.atlasBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.1, 0.1));
  glyphMap_[0x4E16] = gm;

  // "A世B" = 3 logical chars, 5 bytes
  double w = TextLayout::CalculateTextWidth(
      "A\xE4\xB8\x96"
      "B",
      10.0,
      glyphMap_);
  // A(0.5) + 世(1.0) + B(0.5) = 2.0, * 10.0 = 20.0
  EXPECT_DOUBLE_EQ(20.0, w);
}

TEST_F(TextLayoutTest, Utf8VertexGenerationCharCount) {
  GlyphMetrics gm;
  gm.unicode = 0x00E9;
  gm.advance = 0.6;
  gm.planeBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.4, 0.8));
  gm.atlasBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.1, 0.1));
  glyphMap_[0x00E9] = gm;

  std::vector<float> vertexData;
  auto result = TextLayout::GenerateTextVertices(
      "caf\xC3\xA9", 0.0, 0.0, 0.0, 16.0, glyphMap_, 0.8, 0.0, vertexData);
  // 4 logical chars, not 5 bytes
  EXPECT_EQ(4, result.charCount);
  // 4 chars * 6 verts * 6 floats = 144
  EXPECT_EQ(144u, vertexData.size());
}

TEST_F(TextLayoutTest, Utf8MixedMultiByteWidth) {
  // Add glyphs for multi-byte chars
  auto addGlyph = [&](int codepoint, double advance) {
    GlyphMetrics gm;
    gm.unicode = codepoint;
    gm.advance = advance;
    gm.planeBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.4, 0.8));
    gm.atlasBounds = Range2d(Vec2d(0.0, 0.0), Vec2d(0.1, 0.1));
    glyphMap_[codepoint] = gm;
  };

  addGlyph(0x00FC, 0.5); // ü (2-byte)
  addGlyph(0x2603, 0.8); // ☃ (3-byte)

  // "ü☃" = 2 logical chars, 5 bytes
  double w = TextLayout::CalculateTextWidth("\xC3\xBC\xE2\x98\x83", 10.0, glyphMap_);
  EXPECT_DOUBLE_EQ(13.0, w); // (0.5 + 0.8) * 10.0
}

TEST_F(TextLayoutTest, CalculatePortPositionsBasic) {
  std::vector<std::string> inputs = {"x", "y"};
  std::vector<std::string> outputs = {"out"};

  auto positions = TextLayout::CalculatePortPositions(
      inputs, outputs, 14.0, 4.0, Vec2d(0, 0), Vec2d(200, 200), glyphMap_, 16.0, 40.0);

  ASSERT_EQ(3u, positions.size());

  // Input ports on left side (x = marginH)
  EXPECT_DOUBLE_EQ(16.0, positions[0][0]);
  EXPECT_DOUBLE_EQ(16.0, positions[1][0]);

  // Output port on right side
  double textW = TextLayout::CalculateTextWidth("out", 14.0, glyphMap_);
  EXPECT_DOUBLE_EQ(200.0 - 16.0 - textW, positions[2][0]);

  // Y values should increase
  EXPECT_LT(positions[0][1], positions[1][1]);
}

TEST_F(TextLayoutTest, CalculatePortPositionsUseAuthoredRowSlots) {
  std::vector<std::string> inputs = {"inputs", "foo", "middle", "bar"};
  std::vector<std::string> outputs = {"outputs", "left", "right"};
  std::vector<int> inputRowSlots = {0, 1, 4, 5};
  std::vector<int> outputRowSlots = {2, 3, 6};

  auto positions = TextLayout::CalculatePortPositions(
      inputs,
      outputs,
      14.0,
      4.0,
      Vec2d(0, 0),
      Vec2d(200, 200),
      glyphMap_,
      16.0,
      40.0,
      inputRowSlots,
      outputRowSlots);

  ASSERT_EQ(7u, positions.size());

  EXPECT_LT(positions[1][1], positions[4][1]);
  EXPECT_LT(positions[4][1], positions[5][1]);
  EXPECT_LT(positions[5][1], positions[2][1]);
  EXPECT_LT(positions[2][1], positions[3][1]);
  EXPECT_LT(positions[3][1], positions[6][1]);
}

// ---------------------------------------------------------------------------
// GlyphMetrics
// ---------------------------------------------------------------------------

TEST(GlyphMetricsTest, DefaultConstruction) {
  GlyphMetrics gm;
  EXPECT_EQ(0, gm.unicode);
  EXPECT_DOUBLE_EQ(0.0, gm.advance);
}

TEST(GlyphMetricsTest, ParameterizedConstruction) {
  GlyphMetrics gm(
      65, 0.5, Range2d(Vec2d(0, 0), Vec2d(1, 1)), Range2d(Vec2d(0, 0), Vec2d(0.5, 0.5)));
  EXPECT_EQ(65, gm.unicode);
  EXPECT_DOUBLE_EQ(0.5, gm.advance);
}

TEST(TextVertexResultTest, DefaultConstruction) {
  TextVertexResult r;
  EXPECT_DOUBLE_EQ(0.0, r.cursorX);
  EXPECT_EQ(0, r.charCount);
}

TEST(TextVertexResultTest, ParameterizedConstruction) {
  TextVertexResult r(42.5, 10);
  EXPECT_DOUBLE_EQ(42.5, r.cursorX);
  EXPECT_EQ(10, r.charCount);
}

} // namespace noodles
