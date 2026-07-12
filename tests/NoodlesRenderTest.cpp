// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/NodeVertex.h"
#include "render/GraphNodeRenderer.h"
#include "render/LinkGeometry.h"
#include "render/NodeRenderManager.h"
#include "render/NodeVertexCache.h"
#include "render/TextLayout.h"
#include "render/TextRenderManager.h"
#include "render/VertexGenerator.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
  // innerStroke > 0 → the stroke overlay quad adds 6 verts on top of the 12
  // base (bg + title).
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, 1.0f, 0.0f);
  EXPECT_EQ(18u, verts.size());
}

TEST(VertexGeneratorTest, DefaultNodeVertexPositionBounds) {
  Vec2d pos(100, 200);
  Vec2d size(300, 150);
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts =
      VertexGenerator::generateDefaultNodeVertices(pos, size, 0.5f, 40.0f, colors, 1.0f, 0.0f);

  // The center-stroke overlay quad extends halfStroke (innerStroke*0.5 = 0.5)
  // beyond the node edge on all sides.
  float hs = 0.5f;
  for (const auto& v : verts) {
    EXPECT_GE(v.x, 100.0f - hs);
    EXPECT_LE(v.x, 400.0f + hs);
    EXPECT_GE(v.y, 200.0f - hs);
    EXPECT_LE(v.y, 350.0f + hs);
    EXPECT_NEAR(0.5f, v.z, 0.001f);
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
  // With empty rowKinds, all rows are kind 0 (normal): even-parity invisible,
  // odd-parity translucent white overlay.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 4, 20.0f, 35.0f, {});

  // Normal rows: even-parity are invisible (grey 0, alpha 0), odd-parity are a
  // translucent white overlay (grey 255, alpha 30).
  EXPECT_EQ(0, getRowGrey(verts, 0)); // even → invisible
  EXPECT_EQ(255, getRowGrey(verts, 1)); // odd → white overlay
  EXPECT_EQ(0, getRowGrey(verts, 2)); // even → invisible
  EXPECT_EQ(255, getRowGrey(verts, 3)); // odd → white overlay
}

TEST(VertexGeneratorTest, RowColorAlternation_NormalThenUnfoldedGroup) {
  // rowKinds = [0, 2, 3, 3, 0]
  // Normal rows should alternate independently of group/child rows.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 2, 3, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 5, 20.0f, 35.0f, rowKinds);

  // Kind 0/3 alternate among themselves; kind 1/2 headers get bright overlay.
  // rowKinds = [0, 2, 3, 3, 0]: altRowCount covers rows 0,2,3,4 (kinds 0,3,3,0).
  EXPECT_EQ(0, getRowGrey(verts, 0)); // kind 0, alt=0 even: invisible
  EXPECT_EQ(255, getRowGrey(verts, 1)); // kind 2, header: bright white
  EXPECT_EQ(255, getRowGrey(verts, 2)); // kind 3, alt=1 odd: subtle white
  EXPECT_EQ(0, getRowGrey(verts, 3)); // kind 3, alt=2 even: invisible
  EXPECT_EQ(255, getRowGrey(verts, 4)); // kind 0, alt=3 odd: subtle white
}

TEST(VertexGeneratorTest, RowColorAlternation_GroupDoesNotShiftAltParity) {
  // Group headers (kind 1/2) don't increment the alt counter.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 0, 2, 3, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 6, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(0, getRowGrey(verts, 0)); // kind 0, alt=0 even: invisible
  EXPECT_EQ(255, getRowGrey(verts, 1)); // kind 0, alt=1 odd: subtle white
  EXPECT_EQ(255, getRowGrey(verts, 2)); // kind 2, header: bright white
  EXPECT_EQ(0, getRowGrey(verts, 3)); // kind 3, alt=2 even: invisible
  EXPECT_EQ(255, getRowGrey(verts, 4)); // kind 3, alt=3 odd: subtle white
  EXPECT_EQ(0, getRowGrey(verts, 5)); // kind 0, alt=4 even: invisible
}

TEST(VertexGeneratorTest, RowColorAlternation_SelectedColors) {
  // Selected colors follow the same pattern.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {0, 2, 3, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 4, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(0, getRowSelectedGrey(verts, 0)); // kind 0, alt=0: invisible
  EXPECT_EQ(255, getRowSelectedGrey(verts, 1)); // kind 2, header: bright
  EXPECT_EQ(255, getRowSelectedGrey(verts, 2)); // kind 3, alt=1: subtle white
  EXPECT_EQ(0, getRowSelectedGrey(verts, 3)); // kind 0, alt=2: invisible
}

TEST(VertexGeneratorTest, RowColorAlternation_HeadersAlwaysBright) {
  // Group headers (kind 1/2) always get bright white overlay.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {2, 0, 1, 3, 2, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 400), 0.0f, 30.0f, colors, 0.0f, 0.0f, 6, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(255, getRowGrey(verts, 0)); // kind 2 header
  EXPECT_EQ(255, getRowGrey(verts, 2)); // kind 1 header
  EXPECT_EQ(255, getRowGrey(verts, 4)); // kind 2 header
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

  // Populate the cached layout metrics that getPortPosition reads.
  GraphModel model;
  auto textWidth = [](const std::string&, double fontSize) { return fontSize * 5.0; };
  FontMetrics fm{fontAtlas.ascender(), fontAtlas.descender(), fontAtlas.lineHeight()};
  model.calculateNodeSize(node, textWidth, fm, &config);

  const double portLineHeight = node.layoutPortLineHeight;
  const double portStartY = node.position[1] + node.layoutPortStartY;

  const Vec2d inputFoo = renderer.getPortPosition(node, "foo", false, fontAtlas);
  const Vec2d outputLeft = renderer.getPortPosition(node, "left", true, fontAtlas);
  const Vec2d middle = renderer.getPortPosition(node, "middle", false, fontAtlas);
  const Vec2d inputBar = renderer.getPortPosition(node, "bar", false, fontAtlas);

  // Ports sit at the authored row slot's band CENTER (slot * lineHeight + half).
  EXPECT_DOUBLE_EQ(inputFoo[1], portStartY + 1.0 * portLineHeight + portLineHeight * 0.5);
  EXPECT_DOUBLE_EQ(outputLeft[1], portStartY + 3.0 * portLineHeight + portLineHeight * 0.5);
  EXPECT_DOUBLE_EQ(middle[1], portStartY + 4.0 * portLineHeight + portLineHeight * 0.5);
  EXPECT_DOUBLE_EQ(inputBar[1], portStartY + 5.0 * portLineHeight + portLineHeight * 0.5);
  EXPECT_LT(inputFoo[1], outputLeft[1]);
  EXPECT_LT(outputLeft[1], middle[1]);
  EXPECT_LT(middle[1], inputBar[1]);
}

TEST(GraphNodeRendererTest, RenderPathPortsAlignToStripesAfterLayoutNode) {
  // End-to-end render-path alignment (the drift the migration fixes), checked on
  // the REAL render geometry, not a formula: lay out a many-input node with the
  // single producer, generate the actual node-quad stripe vertices, and confirm
  // every port position the circle/link path draws sits at the center of its
  // row's rendered stripe band.
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  FontAtlas fontAtlas;
  GraphModel model;
  auto textWidth = [](const std::string&, double fontSize) { return fontSize * 5.0; };
  FontMetrics fm{fontAtlas.ascender(), fontAtlas.descender(), fontAtlas.lineHeight()};

  NodeData node;
  node.position = Vec2d(10.0, 20.0);
  for (int i = 0; i < 14; ++i) {
    std::string p = "in" + std::to_string(i);
    node.originalInputPins.push_back(p);
    node.orderedPinEntries.emplace_back("input", p);
  }
  model.layoutNode(node, textWidth, fm, &config);
  ASSERT_EQ(14, node.layoutRowCount);

  auto verts = renderer.generateVertices(node, 0.0f, fontAtlas);

  for (int i = 0; i < 14; ++i) {
    int row = node.inputRowSlots[i];
    // Stripe quad for visible row r lives at verts[12 + r*12 .. +6) (12 base
    // quads: bg + title; then 6 stripe + 6 spacer per row).
    int base = 12 + row * 12;
    ASSERT_LE(base + 6, static_cast<int>(verts.size()));
    float top = verts[base].y;
    float bot = verts[base].y;
    for (int k = 0; k < 6; ++k) {
      top = std::min(top, verts[base + k].y);
      bot = std::max(bot, verts[base + k].y);
    }
    double bandCenter = (top + bot) * 0.5;
    Vec2d port = renderer.getPortPosition(node, node.inputPins[i], false, fontAtlas);
    EXPECT_NEAR(bandCenter, port[1], 1e-3)
        << "input " << i << " (row " << row << ") drifted from its stripe band";
  }
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
// LinkGeometry::sampleLinkCurve -- the shaped hit-test curve, which must match
// the GPU vertex shader link_poly_vert.glsl. Expected values below are derived
// by hand from the bezier formulas (constants in LinkCurveParams.h), NOT copied
// from the implementation output.
// ---------------------------------------------------------------------------

TEST(LinkGeometryTest, SampleLinkCurveMinSamples) {
  EXPECT_TRUE(LinkGeometry::sampleLinkCurve(Vec2d(0, 0), Vec2d(100, 0), 0, false).empty());
  EXPECT_TRUE(LinkGeometry::sampleLinkCurve(Vec2d(0, 0), Vec2d(100, 0), 1, false).empty());
}

TEST(LinkGeometryTest, SampleLinkCurveHitsEndpoints) {
  // Every branch starts exactly at `start` and ends exactly at `end`.
  const Vec2d start(12, 34);
  const Vec2d end(-56, 78);
  for (bool vertical : {false, true}) {
    auto curve = LinkGeometry::sampleLinkCurve(start, end, 8, vertical);
    ASSERT_EQ(8u, curve.size());
    EXPECT_NEAR(start[0], curve.front()[0], 1e-9);
    EXPECT_NEAR(start[1], curve.front()[1], 1e-9);
    EXPECT_NEAR(end[0], curve.back()[0], 1e-9);
    EXPECT_NEAR(end[1], curve.back()[1], 1e-9);
  }
}

TEST(LinkGeometryTest, SampleLinkCurveForwardFollowsCosineProfile) {
  // start.x < end.x with small vertical separation keeps the forward<->backward
  // blend at mx == 0, so the curve is the reference profile: x linear in t, y the
  // cosine-squared easing. Here mx_raw arg = (0 - 200 + 550*0.1)*0.5 = -72.5,
  // which clamps to 0 -> mx == 0.
  const Vec2d start(0, 0);
  const Vec2d end(200, 20); // |dy| = 20 -> tOffsetFactor = 0.1
  auto curve = LinkGeometry::sampleLinkCurve(start, end, 9, false);
  ASSERT_EQ(9u, curve.size());
  // Sample i=4 is t=0.5: x = 0.5*200 = 100; cos^2 profile is 0.5 at t=0.5, so
  // y = 0.5*20 = 10.
  EXPECT_NEAR(100.0, curve[4][0], 1e-9);
  EXPECT_NEAR(10.0, curve[4][1], 1e-9);
}

TEST(LinkGeometryTest, SampleLinkCurveBackwardBowsRightOfStart) {
  // Strongly backward link (start.x >> end.x, |dy| >= yMinDist) forces mx == 1,
  // so the curve is the bezier6 S-bow. Control points (LinkCurveParams.h):
  //   d = clamp(500*0.1, 100, 500) = 100
  //   p0=(500,0) p1=(600,0) p2=(600,112.5) p3=(-100,187.5) p4=(-100,300) p5=(0,300)
  //   mid = (250,150)
  // 5 samples -> sample i=1 is t=0.25 -> bezier6 first cubic at its s=0.5:
  //   x = 0.125*500 + 0.375*600 + 0.375*600 + 0.125*250 = 543.75
  //   y = 0.125*0   + 0.375*0   + 0.375*112.5 + 0.125*150 = 60.9375
  const Vec2d start(500, 0);
  const Vec2d end(0, 300);
  auto curve = LinkGeometry::sampleLinkCurve(start, end, 5, false);
  ASSERT_EQ(5u, curve.size());
  EXPECT_NEAR(543.75, curve[1][0], 1e-9);
  EXPECT_NEAR(60.9375, curve[1][1], 1e-9);
  // The bow extends right of the start (x = 500), proving the backward path ran.
  EXPECT_GT(curve[1][0], start[0]);
}

TEST(LinkGeometryTest, SampleLinkCurveVerticalEndTangentEntersVertically) {
  // Prim-target cubic: p0=(10,30) p1=(45,30) p2=(110,0) p3=(110,20).
  //   horizontalHandle = clamp(100*0.35, 20, 120) = 35
  //   verticalHandle   = clamp(10*0.35, 20, 120)  = 20  (clamped up from 3.5)
  // Sample i=4 is t=0.5:
  //   x = 0.125*10 + 0.375*45 + 0.375*110 + 0.125*110 = 73.125
  //   y = 0.125*30 + 0.375*30 + 0.375*0   + 0.125*20  = 17.5
  const Vec2d start(10, 30);
  const Vec2d end(110, 20);
  auto curve = LinkGeometry::sampleLinkCurve(start, end, 9, /*verticalEndTangent=*/true);
  ASSERT_EQ(9u, curve.size());
  EXPECT_NEAR(73.125, curve[4][0], 1e-9);
  EXPECT_NEAR(17.5, curve[4][1], 1e-9);
  // The final approach is more vertical than horizontal (enters from below): the
  // last segment's |dy| exceeds its |dx|.
  const double dx = curve.back()[0] - curve[curve.size() - 2][0];
  const double dy = curve.back()[1] - curve[curve.size() - 2][1];
  EXPECT_GT(dy * dy, dx * dx);
}

TEST(LinkGeometryTest, ComputeLinkCurveBoundsForwardMatchesEndpoints) {
  // Forward link with mx == 0: the curve is the monotone reference profile, so
  // its bounds equal the straight endpoint box.
  Range2d bounds = LinkGeometry::computeLinkCurveBounds(Vec2d(0, 0), Vec2d(200, 20), 9, false);
  EXPECT_NEAR(0.0, bounds.GetMin()[0], 1e-9);
  EXPECT_NEAR(0.0, bounds.GetMin()[1], 1e-9);
  EXPECT_NEAR(200.0, bounds.GetMax()[0], 1e-9);
  EXPECT_NEAR(20.0, bounds.GetMax()[1], 1e-9);
}

TEST(LinkGeometryTest, ComputeLinkCurveBoundsBackwardCoversBow) {
  // Strongly backward link (mx == 1): the S-bow reaches x = 543.75 at t=0.25
  // (see SampleLinkCurveBackwardBowsRightOfStart), right of both endpoints
  // (max endpoint x = 500). The straight endpoint box would stop at 500 and
  // mis-cull the bulge, so the curve bounds must extend past it.
  Range2d bounds = LinkGeometry::computeLinkCurveBounds(Vec2d(500, 0), Vec2d(0, 300), 5, false);
  EXPECT_GT(bounds.GetMax()[0], 500.0);
  EXPECT_NEAR(543.75, bounds.GetMax()[0], 1e-9);
  EXPECT_TRUE(bounds.Contains(Vec2d(500, 0)));
  EXPECT_TRUE(bounds.Contains(Vec2d(0, 300)));
}

TEST(LinkGeometryTest, ComputeLinkCurveBoundsDegenerateFallsBackToEndpoints) {
  // numSamples < 2 cannot sample; falls back to the straight endpoint AABB.
  Range2d bounds = LinkGeometry::computeLinkCurveBounds(Vec2d(10, 40), Vec2d(30, 5), 1, false);
  EXPECT_NEAR(10.0, bounds.GetMin()[0], 1e-9);
  EXPECT_NEAR(5.0, bounds.GetMin()[1], 1e-9);
  EXPECT_NEAR(30.0, bounds.GetMax()[0], 1e-9);
  EXPECT_NEAR(40.0, bounds.GetMax()[1], 1e-9);
}

TEST(LinkGeometryTest, SampleLinkCurveAdaptiveEndpointsExact) {
  const Vec2d start(12, 34);
  const Vec2d end(-56, 78);
  for (bool vertical : {false, true}) {
    auto curve = LinkGeometry::sampleLinkCurveAdaptive(start, end, vertical, 0.5);
    ASSERT_GE(curve.size(), 2u);
    EXPECT_NEAR(start[0], curve.front()[0], 1e-9);
    EXPECT_NEAR(start[1], curve.front()[1], 1e-9);
    EXPECT_NEAR(end[0], curve.back()[0], 1e-9);
    EXPECT_NEAR(end[1], curve.back()[1], 1e-9);
  }
}

TEST(LinkGeometryTest, SampleLinkCurveAdaptiveDenserOnCurvier) {
  // At the same flatness, a near-straight forward link needs far fewer points
  // than a backward S-bow -- the whole point of curvature-adaptive sampling.
  auto straight = LinkGeometry::sampleLinkCurveAdaptive(Vec2d(0, 0), Vec2d(400, 0), false, 0.5);
  auto bow = LinkGeometry::sampleLinkCurveAdaptive(Vec2d(500, 0), Vec2d(0, 300), false, 0.5);
  EXPECT_GT(bow.size(), straight.size());
}

TEST(LinkGeometryTest, SampleLinkCurveAdaptiveStaysWithinFlatness) {
  // The adaptive polyline must track the true curve: every point of a dense
  // uniform sampling lies close to some adaptive segment. (The raw start->end
  // chord would be ~150 world units off for this bow, so a 2-unit bound is a
  // meaningful check that subdivision actually happened.)
  const auto pointSegDistSq = [](const Vec2d& p, const Vec2d& a, const Vec2d& b) {
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double lenSq = dx * dx + dy * dy;
    double t = lenSq > 0.0 ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / lenSq : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double ex = a[0] + t * dx - p[0];
    const double ey = a[1] + t * dy - p[1];
    return ex * ex + ey * ey;
  };
  const Vec2d start(500, 0);
  const Vec2d end(0, 300);
  auto adaptive = LinkGeometry::sampleLinkCurveAdaptive(start, end, false, 0.5);
  ASSERT_GE(adaptive.size(), 2u);
  auto dense = LinkGeometry::sampleLinkCurve(start, end, 400, false);
  for (const Vec2d& d : dense) {
    double best = pointSegDistSq(d, adaptive[0], adaptive[1]);
    for (size_t i = 1; i + 1 < adaptive.size(); ++i) {
      best = std::min(best, pointSegDistSq(d, adaptive[i], adaptive[i + 1]));
    }
    EXPECT_LE(best, 2.0 * 2.0);
  }
}

TEST(LinkGeometryTest, SampleLinkCurveAdaptiveNegativeFlatnessMatchesZero) {
  // A negative tolerance must take the same non-positive fallback branch of
  // the flatnessSq ternary as zero. Pins that equivalence so a refactor that
  // removes the guard -- unconditionally `flatnessSq = flatnessTolerance *
  // flatnessTolerance` -- is caught: for a negative input, squaring produces
  // a POSITIVE `flatnessSq` (1.0), which flips the flatness gate on a curved
  // input, whereas zero-input still maps to `flatnessSq = 0.0`. On the
  // backward S-bow used here the two paths diverge in point count; a
  // straight input would NOT discriminate (dev==0 everywhere makes any
  // non-negative `flatnessSq` gate fire immediately). Robust to whether the
  // sibling bug-fix follow-up (non-positive tolerance mapped to +inf) has
  // landed: under both the +inf and 0.0 mappings, zero and negative take
  // the same branch and produce equal-size, element-equal output.
  const Vec2d start(500, 0);
  const Vec2d end(0, 300);
  auto zero = LinkGeometry::sampleLinkCurveAdaptive(start, end, false, 0.0);
  auto negative = LinkGeometry::sampleLinkCurveAdaptive(start, end, false, -1.0);
  // With kLinkAdaptiveMinDepth = 2, subdivideLinkCurve is forced to split for
  // two levels before flatness is consulted: from [c0, c1] it emits at least
  // 2^kLinkAdaptiveMinDepth + 1 = 5 points on any input. Pinning to `>= 5u`
  // catches both the total short-circuit collapse to [c0, c1] AND a
  // kLinkAdaptiveMinDepth = 1 regression (which would yield exactly 3 points
  // on the S-bow). The prior `> 2u` bound only caught the collapse mutation.
  ASSERT_GE(zero.size(), 5u);
  ASSERT_EQ(zero.size(), negative.size());
  for (size_t i = 0; i < zero.size(); ++i) {
    EXPECT_NEAR(zero[i][0], negative[i][0], 1e-9);
    EXPECT_NEAR(zero[i][1], negative[i][1], 1e-9);
  }
  EXPECT_NEAR(start[0], zero.front()[0], 1e-9);
  EXPECT_NEAR(start[1], zero.front()[1], 1e-9);
  EXPECT_NEAR(end[0], zero.back()[0], 1e-9);
  EXPECT_NEAR(end[1], zero.back()[1], 1e-9);
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

TEST(NodeVertexCacheTest, UpdateSelectedFlagInvalidatesForRegeneration) {
  // Selection changes the vertex count (stroke overlay quad), so
  // updateSelectedFlag marks the node dirty instead of patching in place.
  NodeVertexCache cache;
  std::vector<NodeVertex> verts(6);
  cache.update("node1", verts, Vec2d(0, 0), Vec2d(200, 100), false);

  bool changed = cache.updateSelectedFlag("node1", true, 2.0f);
  EXPECT_TRUE(changed);
  EXPECT_TRUE(cache.isDirty("node1", Vec2d(0, 0), Vec2d(200, 100)));
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

// ---------------------------------------------------------------------------
// NodeRenderManager -- buildCircleEndpointVertices
// ---------------------------------------------------------------------------

TEST(CircleEndpointTest, VertexCount) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      100.0f, 200.0f, 25.0f, 0.5f, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.5f, 0.0f, 1.0f}, 2.0f);
  EXPECT_EQ(6u, verts.size());
}

TEST(CircleEndpointTest, PositionBounds) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      100.0f, 200.0f, 25.0f, 0.5f, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, 2.0f);
  for (const auto& v : verts) {
    EXPECT_GE(v.x, 75.0f);
    EXPECT_LE(v.x, 125.0f);
    EXPECT_GE(v.y, 175.0f);
    EXPECT_LE(v.y, 225.0f);
    EXPECT_FLOAT_EQ(0.5f, v.z);
  }
}

TEST(CircleEndpointTest, UVSpansDiameter) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      100.0f, 200.0f, 25.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, 2.0f);
  float minU = verts[0].u, maxU = verts[0].u;
  float minV = verts[0].v, maxV = verts[0].v;
  for (const auto& vtx : verts) {
    minU = std::min(minU, vtx.u);
    maxU = std::max(maxU, vtx.u);
    minV = std::min(minV, vtx.v);
    maxV = std::max(maxV, vtx.v);
  }
  EXPECT_FLOAT_EQ(0.0f, minU);
  EXPECT_FLOAT_EQ(50.0f, maxU);
  EXPECT_FLOAT_EQ(0.0f, minV);
  EXPECT_FLOAT_EQ(50.0f, maxV);
}

TEST(CircleEndpointTest, SizeIsDiameter) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(20.0f, v.w);
    EXPECT_FLOAT_EQ(20.0f, v.h);
  }
}

TEST(CircleEndpointTest, StrokeColorAndWidth) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.5f}, 3.0f);
  for (const auto& v : verts) {
    EXPECT_EQ(255, v.sr);
    EXPECT_EQ(0, v.sg);
    EXPECT_EQ(0, v.sb);
    EXPECT_EQ(127, v.sa);
    EXPECT_FLOAT_EQ(3.0f, v.innerStroke);
    EXPECT_FLOAT_EQ(-1.0f, v.selected);
  }
}

TEST(CircleEndpointTest, NegativeStrokeWidthClamped) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, -5.0f);
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(0.0f, v.innerStroke);
  }
}

TEST(CircleEndpointTest, FillColorClampNegative) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {-0.5f, -1.0f, -0.01f, -100.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
  for (const auto& v : verts) {
    EXPECT_EQ(0, v.r);
    EXPECT_EQ(0, v.g);
    EXPECT_EQ(0, v.b);
    EXPECT_EQ(0, v.a);
  }
}

TEST(CircleEndpointTest, FillColorClampAboveOne) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {1.5f, 2.0f, 100.0f, 1.01f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
  for (const auto& v : verts) {
    EXPECT_EQ(255, v.r);
    EXPECT_EQ(255, v.g);
    EXPECT_EQ(255, v.b);
    EXPECT_EQ(255, v.a);
  }
}

TEST(CircleEndpointTest, StrokeColorClampNegative) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f}, {-0.5f, -1.0f, -0.01f, -100.0f}, 1.0f);
  for (const auto& v : verts) {
    EXPECT_EQ(0, v.sr);
    EXPECT_EQ(0, v.sg);
    EXPECT_EQ(0, v.sb);
    EXPECT_EQ(0, v.sa);
  }
}

TEST(CircleEndpointTest, StrokeColorClampAboveOne) {
  auto verts = NodeRenderManager::buildCircleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, {0.0f, 0.0f, 0.0f, 1.0f}, {1.5f, 2.0f, 100.0f, 1.01f}, 1.0f);
  for (const auto& v : verts) {
    EXPECT_EQ(255, v.sr);
    EXPECT_EQ(255, v.sg);
    EXPECT_EQ(255, v.sb);
    EXPECT_EQ(255, v.sa);
  }
}

// ---------------------------------------------------------------------------
// NodeRenderManager -- buildTriangleEndpointVertices
// ---------------------------------------------------------------------------

TEST(TriangleEndpointTest, VertexCount) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.5f, {1.0f, 0.0f, 0.0f, 1.0f});
  EXPECT_EQ(3u, verts.size());
}

TEST(TriangleEndpointTest, PositionsMatchInput) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.5f, {1.0f, 0.0f, 0.0f, 1.0f});
  EXPECT_FLOAT_EQ(0.0f, verts[0].x);
  EXPECT_FLOAT_EQ(0.0f, verts[0].y);
  EXPECT_FLOAT_EQ(10.0f, verts[1].x);
  EXPECT_FLOAT_EQ(0.0f, verts[1].y);
  EXPECT_FLOAT_EQ(5.0f, verts[2].x);
  EXPECT_FLOAT_EQ(10.0f, verts[2].y);
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(0.5f, v.z);
  }
}

TEST(TriangleEndpointTest, BoundingBoxUVs) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
  EXPECT_FLOAT_EQ(0.0f, verts[0].u);
  EXPECT_FLOAT_EQ(0.0f, verts[0].v);
  EXPECT_FLOAT_EQ(10.0f, verts[1].u);
  EXPECT_FLOAT_EQ(0.0f, verts[1].v);
  EXPECT_FLOAT_EQ(5.0f, verts[2].u);
  EXPECT_FLOAT_EQ(10.0f, verts[2].v);
}

TEST(TriangleEndpointTest, SizeFromBoundingBox) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(10.0f, v.w);
    EXPECT_FLOAT_EQ(10.0f, v.h);
  }
}

TEST(TriangleEndpointTest, DegenerateSizeClampedToOne) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      5.0f, 0.0f, 5.0f, 0.0f, 5.0f, 10.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(1.0f, v.w);
    EXPECT_FLOAT_EQ(10.0f, v.h);
  }
}

TEST(TriangleEndpointTest, NoStrokeOrSelection) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.0f, {1.0f, 0.5f, 0.0f, 1.0f});
  for (const auto& v : verts) {
    EXPECT_FLOAT_EQ(0.0f, v.innerStroke);
    EXPECT_FLOAT_EQ(0.0f, v.selected);
    EXPECT_EQ(0, v.sr);
    EXPECT_EQ(0, v.sg);
    EXPECT_EQ(0, v.sb);
    EXPECT_EQ(0, v.sa);
  }
}

TEST(TriangleEndpointTest, ColorClampNegative) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.0f, {-0.5f, -1.0f, -0.01f, -100.0f});
  for (const auto& v : verts) {
    EXPECT_EQ(0, v.r);
    EXPECT_EQ(0, v.g);
    EXPECT_EQ(0, v.b);
    EXPECT_EQ(0, v.a);
  }
}

TEST(TriangleEndpointTest, ColorClampAboveOne) {
  auto verts = NodeRenderManager::buildTriangleEndpointVertices(
      0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f, 0.0f, {1.5f, 2.0f, 100.0f, 1.01f});
  for (const auto& v : verts) {
    EXPECT_EQ(255, v.r);
    EXPECT_EQ(255, v.g);
    EXPECT_EQ(255, v.b);
    EXPECT_EQ(255, v.a);
  }
}

// ---------------------------------------------------------------------------
// floatColorToByte clamp (exercised via buildPortQuadVertices)
// ---------------------------------------------------------------------------

TEST(FloatColorToByteTest, NegativeValuesClamped) {
  auto verts = NodeRenderManager::buildPortQuadVertices(
      0.0f, 0.0f, 10.0f, 10.0f, 0.0f, {-0.5f, -1.0f, -100.0f, -0.01f});
  ASSERT_FALSE(verts.empty());
  EXPECT_EQ(0, verts[0].r);
  EXPECT_EQ(0, verts[0].g);
  EXPECT_EQ(0, verts[0].b);
  EXPECT_EQ(0, verts[0].a);
}

TEST(FloatColorToByteTest, OverOneValuesClamped) {
  auto verts = NodeRenderManager::buildPortQuadVertices(
      0.0f, 0.0f, 10.0f, 10.0f, 0.0f, {1.5f, 2.0f, 100.0f, 1.01f});
  ASSERT_FALSE(verts.empty());
  EXPECT_EQ(255, verts[0].r);
  EXPECT_EQ(255, verts[0].g);
  EXPECT_EQ(255, verts[0].b);
  EXPECT_EQ(255, verts[0].a);
}

TEST(FloatColorToByteTest, BoundaryValues) {
  auto verts = NodeRenderManager::buildPortQuadVertices(
      0.0f, 0.0f, 10.0f, 10.0f, 0.0f, {0.0f, 1.0f, 0.5f, 0.0f});
  ASSERT_FALSE(verts.empty());
  EXPECT_EQ(0, verts[0].r);
  EXPECT_EQ(255, verts[0].g);
  EXPECT_EQ(127, verts[0].b);
  EXPECT_EQ(0, verts[0].a);
}

// ---------------------------------------------------------------------------
// Row-color alpha channel + stroke overlay coverage
//
// Pre-land coverage gap (flagged by Donatello): existing row-color tests
// assert grey channel only. Headers (kind 1/2) and odd-parity normal rows
// both use grey=255, so grey-only tests cannot distinguish them. Alpha is
// the primary visual differentiator (header=40, normal-odd=8, even=0). If
// these constants regress, only an alpha assertion catches it.
// ---------------------------------------------------------------------------

// Helper: extract the alpha channel of the first vertex of row i's stripe.
// Layout: 12 base (bg+title) + i * 12 (6 stripe + 6 spacer per row).
static uint8_t getRowAlpha(const std::vector<NodeVertex>& verts, int row) {
  return verts[12 + row * 12].a;
}
static uint8_t getRowSelectedAlpha(const std::vector<NodeVertex>& verts, int row) {
  return verts[12 + row * 12].sa;
}

TEST(VertexGeneratorTest, RowAlpha_HeaderVsNormalDistinguished) {
  // Anti-regression test: header rows MUST have a higher alpha than normal
  // rows, even when both share grey=255. If the alpha constants drift so
  // that headers and odd-parity normal rows look the same, the visual
  // hierarchy collapses silently.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {2, 0, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 3, 20.0f, 35.0f, rowKinds);

  // Row 0: header (kind 2) → alpha 40
  // Row 1: normal kind 0, alt=0 even → invisible (alpha 0)
  // Row 2: normal kind 0, alt=1 odd → subtle (alpha 8)
  EXPECT_EQ(40, getRowAlpha(verts, 0)) << "Header alpha";
  EXPECT_EQ(0, getRowAlpha(verts, 1)) << "Even-parity normal row alpha (invisible)";
  EXPECT_EQ(8, getRowAlpha(verts, 2)) << "Odd-parity normal row alpha (subtle)";
  // The visual-hierarchy contract: headers must be strictly brighter than
  // odd-parity normal rows (which share grey=255).
  EXPECT_GT(getRowAlpha(verts, 0), getRowAlpha(verts, 2))
      << "Header must be more opaque than odd-parity normal row";
}

TEST(VertexGeneratorTest, RowAlpha_SelectedMatchesUnselected) {
  // The selected-state alpha is stored independently in `.sa`. It must
  // mirror the unselected alpha — selection should not change the row
  // overlay opacity, only its color tint (bgHigh/bgLow → selectedBgHigh/Low).
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {2, 0, 0};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 1.0f, 3, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(40, getRowSelectedAlpha(verts, 0));
  EXPECT_EQ(0, getRowSelectedAlpha(verts, 1));
  EXPECT_EQ(8, getRowSelectedAlpha(verts, 2));
}

TEST(VertexGeneratorTest, RowAlpha_ChildPinFollowsAltParity) {
  // Child rows (kind 3) participate in the alt-parity counter alongside
  // normal rows (kind 0). They get the same alpha schedule (0 / 8) — not
  // a fixed value. This pins the post-fix semantics; if a future change
  // re-separates child rows, this test will alert.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  std::vector<int> rowKinds = {3, 3, 3, 3};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), 0.0f, 30.0f, colors, 0.0f, 0.0f, 4, 20.0f, 35.0f, rowKinds);

  EXPECT_EQ(0, getRowAlpha(verts, 0)); // alt=0 even
  EXPECT_EQ(8, getRowAlpha(verts, 1)); // alt=1 odd
  EXPECT_EQ(0, getRowAlpha(verts, 2)); // alt=2 even
  EXPECT_EQ(8, getRowAlpha(verts, 3)); // alt=3 odd
}

// ---------------------------------------------------------------------------
// Selection stroke overlay quad — emission and sentinel value
//
// The original bug being fixed: the inner-stroke selection ring was carried
// on the bg/title vertices, where it was occluded by row/spacer fills. The
// fix emits a dedicated overlay quad on top of all rows, signaled by a
// negative `innerStroke` sentinel that the fragment shader interprets as
// "ring-only, transparent interior".
//
// Existing coverage: `DefaultNodeVertexCount` asserts 18 verts when
// innerStroke=1.0 (12 base + 6 overlay). What's NOT covered:
//   1. The negative case: innerStroke=0 must emit ZERO overlay verts.
//   2. The negative-sentinel value carried by the overlay vertices —
//      if a future refactor drops the negation, the shader's stroke-only
//      branch becomes unreachable and the bug regresses silently.
// ---------------------------------------------------------------------------

TEST(VertexGeneratorTest, NoStrokeOverlayWhenInnerStrokeZero) {
  // No selection → no inner stroke → no overlay quad. The output must be
  // exactly the 12 base verts (bg + title), no more.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, 0.0f, 0.0f);
  EXPECT_EQ(12u, verts.size()) << "innerStroke=0 must NOT emit overlay quad";
}

TEST(VertexGeneratorTest, StrokeOverlayCarriesNegativeSentinel) {
  // The fragment shader keys on `fragInnerStroke < 0` to take the
  // ring-only branch. If the overlay quad emits a positive innerStroke,
  // the shader falls into the fill+inner-stroke branch on the bg/title
  // path, hidden by row fills exactly like the original bug.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  const float kStroke = 8.0f;
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 100), 0.0f, 30.0f, colors, kStroke, 1.0f);

  ASSERT_EQ(18u, verts.size());
  // First 12 verts (bg + title) must NOT carry the stroke — innerStroke
  // was hoisted off them as part of the fix.
  for (int i = 0; i < 12; ++i) {
    EXPECT_FLOAT_EQ(0.0f, verts[i].innerStroke)
        << "bg/title vertex " << i << " must not carry innerStroke";
  }
  // Last 6 verts (overlay quad) must carry the NEGATIVE sentinel.
  for (int i = 12; i < 18; ++i) {
    EXPECT_FLOAT_EQ(-kStroke, verts[i].innerStroke)
        << "overlay vertex " << i << " must carry negated stroke as sentinel";
  }
}

TEST(VertexGeneratorTest, StrokeOverlayDrawnOnTopOfRows) {
  // The overlay's depth must be ABOVE the row stripe and spacer depths,
  // otherwise the row/spacer quads will occlude the stroke ring (the
  // original bug). With pinCount=2: stripeDepth = baseDepth + 0.00003,
  // spacerDepth = baseDepth + 0.00006, overlayDepth = baseDepth + 0.00009.
  DefaultNodeColors colors{50, 40, 248, 45, 35,  70,  60, 65,  55, 100, 80,
                           60, 90, 70,  50, 120, 100, 80, 110, 90, 70};
  const float kBaseDepth = 0.5f;
  auto verts = VertexGenerator::generateDefaultNodeVertices(
      Vec2d(0, 0), Vec2d(200, 200), kBaseDepth, 30.0f, colors, 8.0f, 0.0f, 2, 20.0f, 35.0f, {});

  // 12 bg+title + 2*6 stripes + 1*6 spacer + 6 overlay = 36
  ASSERT_EQ(36u, verts.size());
  // The overlay's z must be strictly greater than every row/spacer z.
  float overlayZ = verts[35].z;
  for (int i = 12; i < 30; ++i) {
    EXPECT_LT(verts[i].z, overlayZ) << "row/spacer vertex " << i << " must be behind the overlay";
  }
}

// ---------------------------------------------------------------------------
// NodeRenderManager::buildPortHighlightVertices (C++ port-highlight producer)
// ---------------------------------------------------------------------------

namespace {
LinkData makeHighlightLink(
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

NodeData makeHighlightNode() {
  NodeData node;
  node.id = "N";
  node.position = Vec2d(100.0, 200.0);
  node.size = Vec2d(80.0, 60.0);
  return node;
}
} // namespace

TEST(PortHighlightProducerTest, ConnectedInputPortEmitsCircleAtRowCenter) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "in")};

  RenderConfig config;
  PortHighlightState state;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, state);

  ASSERT_EQ(6u, v.regular.size()); // one circle = two triangles
  EXPECT_TRUE(v.relationship.empty());
  // portRadius = nodePortWidth/2 = 20; center = (100, 200 + 10); quad TL = center - radius.
  EXPECT_FLOAT_EQ(80.0f, v.regular[0].x);
  EXPECT_FLOAT_EQ(190.0f, v.regular[0].y);
  EXPECT_FLOAT_EQ(40.0f, v.regular[0].w);
  // Connected fill color [0.7, 0.8, 0.0] -> bytes; per-vertex-stroke sentinel.
  EXPECT_EQ(178, v.regular[0].r);
  EXPECT_EQ(204, v.regular[0].g);
  EXPECT_EQ(0, v.regular[0].b);
  EXPECT_FLOAT_EQ(-1.0f, v.regular[0].selected);
}

TEST(PortHighlightProducerTest, PortsFollowNodeDragOffset) {
  // A node moves via the shared transform texture during a drag; the producer
  // must shift its absolute port vertices by the same live offset so glyphs stay
  // attached. With no transform frame the port sits at the base position; with a
  // translated frame it shifts by exactly the offset (relative attrs unchanged).
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "in")};

  RenderConfig config;
  PortHighlightState state;

  auto base = NodeRenderManager::buildPortHighlightVertices(graph, config, state, nullptr);
  ASSERT_EQ(6u, base.regular.size());

  NodeTransformFrame frame;
  frame.syncFromNodes(graph.nodes); // assign the node a slot (GL-free), as production does
  frame.translate("N", 25.0, -15.0); // simulate a drag delta
  auto moved = NodeRenderManager::buildPortHighlightVertices(graph, config, state, &frame);
  ASSERT_EQ(6u, moved.regular.size());

  EXPECT_FLOAT_EQ(base.regular[0].x + 25.0f, moved.regular[0].x);
  EXPECT_FLOAT_EQ(base.regular[0].y - 15.0f, moved.regular[0].y);
  EXPECT_FLOAT_EQ(base.regular[0].w, moved.regular[0].w); // relative attr unchanged
}

TEST(PortHighlightProducerTest, PortsStableAfterDragReleaseResync) {
  // Post-release state: the node has moved, the snapshot position is re-synced to
  // the new live position, but the transform frame keeps the ORIGINAL base + the
  // accumulated offset (syncFromNodes never re-bases an existing node). base +
  // offset already equals the snapshot, so the producer must shift by 0 — not by
  // the offset again, which would double-count and strand the glyph.
  GraphModel graph;
  NodeData node = makeHighlightNode(); // base / original (100, 200)
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "in")};

  RenderConfig config;
  PortHighlightState state;

  NodeTransformFrame frame;
  frame.syncFromNodes(graph.nodes); // base = (100, 200), offset = 0
  frame.translate("N", 25.0, -15.0); // offset = (25, -15); true pos = (125, 185)
  // Simulate the post-release re-sync: the snapshot now holds the live position.
  graph.nodes.at("N").position = Vec2d(125.0, 185.0);

  auto noFrame = NodeRenderManager::buildPortHighlightVertices(graph, config, state, nullptr);
  auto withFrame = NodeRenderManager::buildPortHighlightVertices(graph, config, state, &frame);
  ASSERT_EQ(6u, noFrame.regular.size());
  ASSERT_EQ(6u, withFrame.regular.size());
  // base + offset == snapshot, so no shift: the glyph stays at the snapshot
  // position rather than overshooting by the drag delta.
  EXPECT_FLOAT_EQ(noFrame.regular[0].x, withFrame.regular[0].x);
  EXPECT_FLOAT_EQ(noFrame.regular[0].y, withFrame.regular[0].y);
}

// renderPortHighlightsFromGraph uses hashPortHighlightVertices as the VBO
// "generation" so the re-upload is skipped when the highlight geometry is
// unchanged. These cover the change-detector: equal geometry must hash equal (so
// idle / pan / zoom frames skip the upload), and any geometry change must alter
// the hash (so hover / drag / connection / fold / move force a re-upload).
TEST(PortHighlightHashTest, IdenticalGeometryHashesEqual) {
  std::vector<NodeVertex> a;
  a.emplace_back();
  a.back().x = 10.0f;
  a.back().y = 20.0f;
  a.back().r = 128;
  std::vector<NodeVertex> b = a;
  EXPECT_EQ(
      NodeRenderManager::hashPortHighlightVertices(a),
      NodeRenderManager::hashPortHighlightVertices(b));
}

TEST(PortHighlightHashTest, PositionChangeHashesDiffer) {
  std::vector<NodeVertex> a;
  a.emplace_back();
  a.back().x = 10.0f;
  std::vector<NodeVertex> b = a;
  b.back().x = 11.0f; // a node drag moves the glyph
  EXPECT_NE(
      NodeRenderManager::hashPortHighlightVertices(a),
      NodeRenderManager::hashPortHighlightVertices(b));
}

TEST(PortHighlightHashTest, ColorChangeHashesDiffer) {
  std::vector<NodeVertex> a;
  a.emplace_back();
  a.back().r = 10;
  std::vector<NodeVertex> b = a;
  b.back().r = 200; // hover / connection-state recolor
  EXPECT_NE(
      NodeRenderManager::hashPortHighlightVertices(a),
      NodeRenderManager::hashPortHighlightVertices(b));
}

TEST(PortHighlightHashTest, VertexCountChangeHashesDiffer) {
  std::vector<NodeVertex> a;
  a.emplace_back();
  std::vector<NodeVertex> b = a;
  b.emplace_back(); // a port appeared (e.g. fold expanded)
  EXPECT_NE(
      NodeRenderManager::hashPortHighlightVertices(a),
      NodeRenderManager::hashPortHighlightVertices(b));
}

TEST(PortHighlightHashTest, EmptyBatchHashIsStable) {
  EXPECT_EQ(
      NodeRenderManager::hashPortHighlightVertices({}),
      NodeRenderManager::hashPortHighlightVertices({}));
}

// renderPortHighlightsFromGraph reuses its cached geometry when the hover /
// link-drag state is unchanged (detected via PortHighlightState equality), so a
// pan / zoom skips the O(ports) rebuild. These lock in that comparison.
TEST(PortHighlightStateTest, EqualStatesCompareEqual) {
  PortHighlightState a;
  a.hasHover = true;
  a.hoveredNodeId = "N";
  a.hoveredPort = "in";
  a.hoveredIsOutput = false;
  PortHighlightState b = a;
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(PortHighlightStateTest, HoverChangeComparesUnequal) {
  PortHighlightState a;
  a.hasHover = true;
  a.hoveredNodeId = "N";
  a.hoveredPort = "in";
  PortHighlightState b = a;
  b.hoveredPort = "out"; // cursor moved to a different port
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(PortHighlightStateTest, DragStateChangeComparesUnequal) {
  PortHighlightState a;
  PortHighlightState b;
  b.draggingLink = true;
  b.dragSourceNodeId = "N";
  b.dragSourcePort = "out";
  b.dragSourceIsOutput = true;
  EXPECT_TRUE(a != b);
}

TEST(PortHighlightProducerTest, DisconnectedPortHiddenUnlessDragSource) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  // No links: the port is disconnected.
  RenderConfig config;

  PortHighlightState idle;
  EXPECT_TRUE(NodeRenderManager::buildPortHighlightVertices(graph, config, idle).regular.empty());

  PortHighlightState dragging;
  dragging.draggingLink = true;
  dragging.dragSourceNodeId = "N";
  dragging.dragSourcePort = "in";
  dragging.dragSourceIsOutput = false;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, dragging);
  ASSERT_EQ(6u, v.regular.size());
  EXPECT_EQ(59, v.regular[0].r); // disconnected fill [0.235, ...] -> 59
}

TEST(PortHighlightProducerTest, RelationshipRowEmitsTriangles) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"sources"};
  node.layoutInputCenterY = {10.0};
  node.relationshipInputPins = {"sources"};
  graph.nodes.emplace("N", node);
  // Empty targetPropertyName -> target classifies to the input side, where the
  // "sources" row is visible, so this exercises the per-row relationship glyph
  // (triangles) without also producing a synthetic port.
  graph.links = {makeHighlightLink("A", "out", "N", "sources", /*isRelationship=*/true, "")};

  RenderConfig config;
  PortHighlightState state;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, state);

  EXPECT_TRUE(v.regular.empty());
  EXPECT_EQ(6u, v.relationship.size()); // outer + inner triangle, 3 verts each
}

TEST(PortHighlightProducerTest, HoverUsesHoverRingAndDoublesThickness) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "in")};

  RenderConfig config;
  PortHighlightState state;
  state.hasHover = true;
  state.hoveredNodeId = "N";
  state.hoveredPort = "in";
  state.hoveredIsOutput = false;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, state);

  ASSERT_EQ(6u, v.regular.size());
  // Hovering doubles the ring thickness (stored in innerStroke) and recolors the
  // ring to the hover color [0.4, 0.7, 1.0].
  EXPECT_FLOAT_EQ(static_cast<float>(config.nodePortRingThickness * 2.0), v.regular[0].innerStroke);
  EXPECT_EQ(102, v.regular[0].sr);
  EXPECT_EQ(178, v.regular[0].sg);
  EXPECT_EQ(255, v.regular[0].sb);
}

TEST(PortHighlightProducerTest, TitleCollapsedEmitsAggregatePort) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.titleCollapsed = true;
  node.originalInputPins = {"a"};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "a")};

  RenderConfig config;
  PortHighlightState state;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, state);

  ASSERT_EQ(6u, v.regular.size()); // single aggregate input port
  // Aggregate port sits at the left edge, vertical node center: (100, 200 + 30).
  EXPECT_FLOAT_EQ(80.0f, v.regular[0].x); // 100 - radius(20)
  EXPECT_FLOAT_EQ(210.0f, v.regular[0].y); // 230 - radius(20)
}

TEST(PortHighlightProducerTest, SyntheticRelationshipPortEmitsTriangle) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.layoutTitleHeight = 20.0;
  // No display pins: the relationship endpoint has no visible row, so it draws a
  // synthetic aggregate handle near the title.
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "sources", /*isRelationship=*/true, "sources")};

  RenderConfig config;
  PortHighlightState state;
  auto v = NodeRenderManager::buildPortHighlightVertices(graph, config, state);

  EXPECT_TRUE(v.regular.empty());
  EXPECT_EQ(6u, v.relationship.size());
}

// ---------------------------------------------------------------------------
// NodeRenderManager::portAtPoint (unified C++ port hit-test)
// ---------------------------------------------------------------------------
// Geometry shared by the cases below: makeHighlightNode() is at (100, 200) with
// size (80, 60), so a pin with layoutCenterY 10 sits at world Y 210, the left
// (input) edge at X 100 and the right (output) edge at X 180. A hit radius of 20
// (the drawn port radius) accepts a cursor on the glyph center.

TEST(PortAtPointTest, FindsConnectedInputPort) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "in")};

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("N", hit.nodeId);
  EXPECT_EQ("in", hit.portName);
  EXPECT_FALSE(hit.isOutput);
}

TEST(PortAtPointTest, OccludedPortIsNotPickable) {
  // A lower node's input port (at world (100, 210)) sits under a higher node's
  // body, so it is visually hidden and must not be pickable -- it was, before
  // portAtPoint respected z-order.
  GraphModel graph;
  NodeData low = makeHighlightNode(); // (100, 200), size (80, 60)
  low.zOrder = 0;
  low.inputPins = {"in"};
  low.layoutInputCenterY = {10.0}; // port at world (100, 210)
  graph.nodes.emplace("low", low);

  NodeData high = makeHighlightNode();
  high.zOrder = 1; // drawn on top
  high.position = Vec2d(60.0, 180.0); // body covers X[60,140] Y[180,240] -> (100,210)
  graph.nodes.emplace("high", high);

  // The high node has no port within radius 20 of (100,210) (its edges are at
  // X 60 / 140), so the only candidate is the occluded low port -> no hit.
  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0);
  EXPECT_FALSE(hit.found);
}

TEST(PortAtPointTest, NonOccludedPortStillPickable) {
  // Same lower node, but the higher node is elsewhere and covers nothing near the
  // port -- it stays pickable, so the occlusion check does not over-reject.
  GraphModel graph;
  NodeData low = makeHighlightNode();
  low.zOrder = 0;
  low.inputPins = {"in"};
  low.layoutInputCenterY = {10.0}; // port at world (100, 210)
  graph.nodes.emplace("low", low);

  NodeData high = makeHighlightNode();
  high.zOrder = 1;
  high.position = Vec2d(400.0, 400.0); // far away; covers nothing near (100,210)
  graph.nodes.emplace("high", high);

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("low", hit.nodeId);
  EXPECT_EQ("in", hit.portName);
}

TEST(PortAtPointTest, FindsDisconnectedPort) {
  // Unlike the highlight producer (which hides unconnected ports), the hit-test
  // still returns a disconnected pin — it is a valid hover / link-drag start.
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node); // no links: the port is disconnected

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("in", hit.portName);
  EXPECT_FALSE(hit.isOutput);
}

TEST(PortAtPointTest, MissesWhenFarFromAnyPort) {
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);

  // Node center X (140) is 40 from each edge — outside the radius-20 port circles.
  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(140.0, 210.0), 20.0);
  EXPECT_FALSE(hit.found);
}

TEST(PortAtPointTest, FindsDualPinMirrorOnRightEdge) {
  // A dual input pin also exposes a mirrored OUTPUT port on the right edge that
  // the old per-node getPortAtPoint missed (it is not in outputPins).
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"dual"};
  node.layoutInputCenterY = {10.0};
  node.dualPinNames = {"dual"};
  graph.nodes.emplace("N", node);

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(180.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("dual", hit.portName);
  EXPECT_TRUE(hit.isOutput);
}

TEST(PortAtPointTest, FindsOutputDualMirrorOnLeftEdge) {
  // Symmetric: an output-dual pin exposes a mirrored INPUT port on the left edge.
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.outputPins = {"od"};
  node.layoutOutputCenterY = {10.0};
  node.outputDualPinNames = {"od"};
  graph.nodes.emplace("N", node);

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("od", hit.portName);
  EXPECT_FALSE(hit.isOutput);
}

TEST(PortAtPointTest, FindsSyntheticRelationshipPort) {
  // A relationship endpoint with no visible row is hittable as the synthetic
  // aggregate handle near the title (right/output edge, title center).
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.layoutTitleHeight = 20.0; // title center at world Y 200 + 10 = 210
  graph.nodes.emplace("N", node);
  graph.links = {makeHighlightLink("A", "out", "N", "sources", /*isRelationship=*/true, "sources")};

  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(180.0, 210.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("sources", hit.portName);
  EXPECT_TRUE(hit.isOutput);
}

TEST(PortAtPointTest, ResolvesCollapsedAggregateToFirstPin) {
  // A collapsed node draws one aggregate handle per side; the hit-test resolves it
  // back to a concrete pin so callers get a real port name (not the sentinel).
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.titleCollapsed = true;
  node.inputPins = {"a"};
  node.originalInputPins = {"a"};
  graph.nodes.emplace("N", node);

  // Aggregate input sits at the left edge, vertical node center (200 + 30 = 230).
  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 230.0), 20.0);
  EXPECT_TRUE(hit.found);
  EXPECT_EQ("a", hit.portName);
  EXPECT_FALSE(hit.isOutput);
}

TEST(PortAtPointTest, PortFollowsNodeDragOffset) {
  // With a translated transform frame the hit region tracks the live (dragged)
  // position, exactly like the drawn circle — base (100, 210) becomes (125, 195).
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  graph.nodes.emplace("N", node);

  NodeTransformFrame frame;
  frame.syncFromNodes(graph.nodes);
  frame.translate("N", 25.0, -15.0);

  auto atLive = NodeRenderManager::portAtPoint(graph, Vec2d(125.0, 195.0), 20.0, &frame);
  EXPECT_TRUE(atLive.found);
  EXPECT_EQ("in", atLive.portName);

  // The pre-drag base position is no longer a hit (the port moved with the node).
  auto atBase = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 210.0), 20.0, &frame);
  EXPECT_FALSE(atBase.found);
}

TEST(PortAtPointTest, TwoNodeEquidistantTiesPickDeterministically) {
  // Two overlapping nodes each expose an input port whose glyph center is the same
  // world point. The old Python hover loop prepended nodeIdUnderCursor to the
  // candidate list and returned on the first hit, so the under-cursor node won a
  // tie. portAtPoint has no such hint and compares by squared distance only, so a
  // tie is resolved by whichever glyph the walk visits first — i.e. by
  // unordered_map iteration order (< is strict, so the FIRST equidistant glyph
  // wins). This test does not pin which node wins (that would be a change-detector
  // against libstdc++'s hashing) — it pins the stability-across-calls invariant
  // and the sanity check that the winner is one of the real candidates. Any
  // future refactor that introduces intra-call randomness (e.g. threading the
  // outer loop) breaks it.
  GraphModel graph;

  NodeData nodeLeft = makeHighlightNode();
  nodeLeft.inputPins = {"in"};
  nodeLeft.layoutInputCenterY = {30.0}; // world center: (100, 230)

  NodeData nodeRight = makeHighlightNode();
  nodeRight.inputPins = {"in"};
  nodeRight.layoutInputCenterY = {30.0}; // same world center — abutting overlap

  graph.nodes.emplace("L", nodeLeft);
  graph.nodes.emplace("R", nodeRight);

  auto a = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 230.0), 20.0);
  auto b = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 230.0), 20.0);
  EXPECT_TRUE(a.found);
  EXPECT_TRUE(b.found);
  // Same input, same graph → same winner across calls (no intra-frame randomness).
  EXPECT_EQ(a.nodeId, b.nodeId);
  EXPECT_EQ(a.portName, b.portName);
  EXPECT_EQ(a.isOutput, b.isOutput);
  // Winner must be one of the two real candidates, on the "in" input pin.
  EXPECT_TRUE(a.nodeId == "L" || a.nodeId == "R");
  EXPECT_EQ("in", a.portName);
  EXPECT_FALSE(a.isOutput);
}

// KNOWN-BUG PIN — pins the current emit/resolve asymmetry (see below) as an
// executable tripwire. A paired resolver-fix diff from the same PLDR run will
// flip hit.found from false → true; when that fix lands, this test WILL fail
// and MUST be updated in the same commit (flip EXPECT_FALSE → EXPECT_TRUE and
// add EXPECT_EQ("a", hit.portName)) to keep both sides of the emit/resolve
// contract in one place. Do not silently delete or disable — the failure IS
// the signal.
TEST(PortAtPointTest, CollapsedAggregateWithEmptyInputPinsDoesNotResolve) {
  // forEachPortOnNode's title-collapsed branch emits the aggregate glyph when
  // origIn = (originalInputPins.empty ? inputPins : originalInputPins) is
  // non-empty — so a collapsed node with inputPins = {} and originalInputPins =
  // {"a"} DOES emit an aggregate glyph. But portAtPoint's aggregate resolver
  // reads glyph.node.inputPins unconditionally and early-returns when it is
  // empty, silently dropping the hit. This test pins that observable behavior as
  // an executable contract for the emit/resolve asymmetry — paired with the
  // sibling ResolvesCollapsedAggregateToFirstPin (which asserts the positive
  // path with inputPins = originalInputPins = {"a"}), the two together document
  // the current contract. Any future fix that mirrors the emit-side fallback
  // will break this test and force the author to update the header comment on
  // portAtPoint (which currently says "resolves ... to its first display pin")
  // and the paired sibling in one place instead of two.
  GraphModel graph;
  NodeData node = makeHighlightNode();
  node.titleCollapsed = true;
  node.inputPins = {}; // display list collapsed away
  node.originalInputPins = {"a"}; // producer emits based on this
  graph.nodes.emplace("N", node);

  // Aggregate input sits at the left edge, vertical node center (200 + 30 = 230).
  auto hit = NodeRenderManager::portAtPoint(graph, Vec2d(100.0, 230.0), 20.0);
  EXPECT_FALSE(hit.found);
}

// ---------------------------------------------------------------------------
// GraphNodeRenderer::resolvePortPosition (full port-position resolver)
// ---------------------------------------------------------------------------
// Same geometry as PortAtPointTest: makeHighlightNode() is (100, 200) size
// (80, 60); a pin with layoutCenterY 10 sits at world Y 210, input edge X 100,
// output edge X 180.

TEST(ResolvePortPositionTest, NormalPinsUseEdgeAndRowCenter) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};
  node.outputPins = {"out"};
  node.layoutOutputCenterY = {10.0};

  Vec2d inPos = renderer.resolvePortPosition(node, "in", false);
  EXPECT_DOUBLE_EQ(100.0, inPos[0]);
  EXPECT_DOUBLE_EQ(210.0, inPos[1]);
  Vec2d outPos = renderer.resolvePortPosition(node, "out", true);
  EXPECT_DOUBLE_EQ(180.0, outPos[0]);
  EXPECT_DOUBLE_EQ(210.0, outPos[1]);
}

TEST(ResolvePortPositionTest, DualPinMirrorsToRightEdge) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.inputPins = {"dual"};
  node.layoutInputCenterY = {10.0};
  node.dualPinNames = {"dual"};

  Vec2d outPos = renderer.resolvePortPosition(node, "dual", true);
  EXPECT_DOUBLE_EQ(180.0, outPos[0]); // mirrored to the right edge
  EXPECT_DOUBLE_EQ(210.0, outPos[1]); // same row center
}

TEST(ResolvePortPositionTest, OutputDualPinMirrorsToLeftEdge) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.outputPins = {"od"};
  node.layoutOutputCenterY = {10.0};
  node.outputDualPinNames = {"od"};

  Vec2d inPos = renderer.resolvePortPosition(node, "od", false);
  EXPECT_DOUBLE_EQ(100.0, inPos[0]); // mirrored to the left edge
  EXPECT_DOUBLE_EQ(210.0, inPos[1]);
}

TEST(ResolvePortPositionTest, SyntheticRelationshipPortAtTitle) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.layoutTitleHeight = 20.0; // title center at world Y 210
  // "sources" is a relationship pin with no visible row -> title-area handle.

  Vec2d outPos = renderer.resolvePortPosition(node, "sources", true);
  EXPECT_DOUBLE_EQ(180.0, outPos[0]); // right edge (output)
  EXPECT_DOUBLE_EQ(210.0, outPos[1]); // title center
  Vec2d inPos = renderer.resolvePortPosition(node, "sources", false);
  EXPECT_DOUBLE_EQ(100.0, inPos[0]); // left edge (input)
  EXPECT_DOUBLE_EQ(210.0, inPos[1]);
}

TEST(ResolvePortPositionTest, VisibleRelationshipPinUsesRowNotTitle) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.layoutTitleHeight = 20.0;
  node.inputPins = {"sources"}; // relationship pin WITH a visible row
  node.layoutInputCenterY = {40.0};

  Vec2d pos = renderer.resolvePortPosition(node, "sources", false);
  EXPECT_DOUBLE_EQ(100.0, pos[0]);
  EXPECT_DOUBLE_EQ(240.0, pos[1]); // row center (200 + 40), not the title
}

TEST(ResolvePortPositionTest, UnknownPinFallsBackToNodeCenter) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.inputPins = {"in"};
  node.layoutInputCenterY = {10.0};

  Vec2d pos = renderer.resolvePortPosition(node, "missing", false);
  EXPECT_DOUBLE_EQ(100.0, pos[0]); // left edge
  EXPECT_DOUBLE_EQ(230.0, pos[1]); // node vertical center (200 + 60/2)
}

// Lock-down: dual pin present in dualPinNames but NOT in inputPins (e.g. folded
// away) — the mirror is computed against pinCircleCenter's node-center fallback
// rather than a row center. Pre-diff Python had `if inPos:` guards that treated
// this as a fallthrough; the C++ path always returns the mirrored node-center.
// Locks in the current silent-fallback contract so a future assert or return-
// null change is a visible regression.
TEST(ResolvePortPositionTest, DualPinNotInInputPinsMirrorsNodeCenterFallback) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode();
  node.dualPinNames = {"folded"}; // in the set, but NOT in inputPins.

  Vec2d outPos = renderer.resolvePortPosition(node, "folded", true);
  // pinCircleCenter(node, "folded", false) returns (100, 230) — left edge X,
  // node-center Y — because the pin isn't in inputPins. The mirror across the
  // node midline reflects the left edge to the right edge.
  EXPECT_DOUBLE_EQ(180.0, outPos[0]); // right edge (mirrored left edge)
  EXPECT_DOUBLE_EQ(230.0, outPos[1]); // node vertical center (200 + 60/2)
}

// Lock-down: synthetic relationship port with an unset (0.0) layoutTitleHeight
// uses the min(size[1], 32.0) clamp instead of a hard-coded default. Guards the
// specific spec — a future refactor to a constant default would silently drift
// title-area handle placement on small nodes.
TEST(ResolvePortPositionTest, SyntheticRelationshipPortClampsWithoutLayoutHeight) {
  RenderConfig config;
  DefaultNodeRenderer renderer(config);
  NodeData node = makeHighlightNode(); // size (80, 60), layoutTitleHeight = 0.
  // "sources" is a relationship pin, no visible row -> synthetic path.

  Vec2d pos = renderer.resolvePortPosition(node, "sources", true);
  // min(size[1] = 60, 32.0) = 32.0, so center Y = position[1] + 32/2 = 216.
  EXPECT_DOUBLE_EQ(180.0, pos[0]); // right edge (output)
  EXPECT_DOUBLE_EQ(216.0, pos[1]); // node.position[1] + 32/2

  NodeData tinyNode = makeHighlightNode();
  tinyNode.size = Vec2d(80.0, 10.0); // size[1] < 32 -> clamp uses size[1].
  Vec2d tinyPos = renderer.resolvePortPosition(tinyNode, "sources", true);
  EXPECT_DOUBLE_EQ(180.0, tinyPos[0]);
  EXPECT_DOUBLE_EQ(205.0, tinyPos[1]); // 200 + 10/2 (size[1] used as clamp).
}

// ---------------------------------------------------------------------------
// TextRenderManager::computeVisibleTextRanges (off-screen text culling)
// ---------------------------------------------------------------------------

namespace {
NodeData makeTextCullNode(double x, double y, double w, double h) {
  NodeData node;
  node.position = Vec2d(x, y);
  node.size = Vec2d(w, h);
  return node;
}
} // namespace

TEST(TextCullTest, DrawsOnlyIntersectingNodeRanges) {
  // A is on-screen, B is far off-screen; only A's glyph range is emitted. The
  // range is (first = startFloat / 6, count = numChars * 6).
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0);
  nodes["B"] = makeTextCullNode(1000.0, 1000.0, 100.0, 100.0);
  std::unordered_map<std::string, std::pair<int, int>> slices;
  slices["A"] = {0, 3}; // start float 0, 3 chars
  slices["B"] = {108, 2}; // start float 108 (= 3 chars * 36 floats), 2 chars

  std::vector<GLint> firsts;
  std::vector<GLsizei> counts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, -10.0, -10.0, 200.0, 200.0, firsts, counts);

  const std::vector<GLint> expectedFirsts{0}; // 0 / 6
  const std::vector<GLsizei> expectedCounts{18}; // 3 * 6
  EXPECT_EQ(expectedFirsts, firsts);
  EXPECT_EQ(expectedCounts, counts);
}

TEST(TextCullTest, SkipsNodesWithoutDrawableGeometry) {
  // Inside the viewport but: B has no slice, C has a zero-char slice — neither
  // contributes a range; only A does.
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0);
  nodes["B"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0);
  nodes["C"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0);
  std::unordered_map<std::string, std::pair<int, int>> slices;
  slices["A"] = {0, 1};
  slices["C"] = {36, 0};

  std::vector<GLint> firsts;
  std::vector<GLsizei> counts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, -1000.0, -1000.0, 1000.0, 1000.0, firsts, counts);

  EXPECT_EQ((std::vector<GLint>{0}), firsts);
  EXPECT_EQ((std::vector<GLsizei>{6}), counts);
}

TEST(TextCullTest, KeepsNodeStraddlingViewportEdge) {
  // A spans (-50, -50)..(50, 50); the viewport starts at the origin, so the node
  // overlaps the edge and must be kept (cull is conservative, never clips).
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeTextCullNode(-50.0, -50.0, 100.0, 100.0);
  std::unordered_map<std::string, std::pair<int, int>> slices;
  slices["A"] = {0, 2};

  std::vector<GLint> firsts;
  std::vector<GLsizei> counts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, 0.0, 0.0, 500.0, 500.0, firsts, counts);

  EXPECT_EQ((std::vector<GLint>{0}), firsts);
  EXPECT_EQ((std::vector<GLsizei>{12}), counts);
}

TEST(TextCullTest, CullsByLiveTransformOffsetNotStaleSnapshot) {
  // A node's glyphs bake at the transform-frame base and ride the live move
  // offset on the GPU, but the GraphModel snapshot (node.position) is NOT
  // re-synced on a drag. Culling on the snapshot would drop a node the user
  // dragged on-screen ("text disappears while visible after drag + pan"); the
  // cull must use base + offset, matching where the glyphs actually draw.
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0); // snapshot off the viewport
  std::unordered_map<std::string, std::pair<int, int>> slices;
  slices["A"] = {0, 2};

  NodeTransformFrame frame;
  frame.syncFromNodes(nodes); // base = (0, 0)
  frame.translate("A", 300.0, 300.0); // live move: base + offset = (300, 300)

  const double vpMinX = 200.0;
  const double vpMinY = 200.0;
  const double vpMaxX = 400.0;
  const double vpMaxY = 400.0;

  // Stale-snapshot cull (no frame): the node reads as off-screen and is dropped.
  std::vector<GLint> staleFirsts;
  std::vector<GLsizei> staleCounts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, vpMinX, vpMinY, vpMaxX, vpMaxY, staleFirsts, staleCounts);
  EXPECT_TRUE(staleFirsts.empty()) << "snapshot (0,0) is outside the viewport";

  // Transform-aware cull: base + offset = (300, 300) is inside the viewport, so
  // the dragged node's text is kept.
  std::vector<GLint> firsts;
  std::vector<GLsizei> counts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, vpMinX, vpMinY, vpMaxX, vpMaxY, firsts, counts, &frame);
  EXPECT_EQ((std::vector<GLint>{0}), firsts);
  EXPECT_EQ((std::vector<GLsizei>{12}), counts);
}

TEST(TextCullTest, CullsNodeDraggedOffViewport) {
  // The reverse: a node whose snapshot is on-screen but whose live drag moved it
  // out must be culled on base + offset (matching the glyphs, now off-screen).
  std::unordered_map<std::string, NodeData> nodes;
  nodes["A"] = makeTextCullNode(0.0, 0.0, 100.0, 100.0);
  std::unordered_map<std::string, std::pair<int, int>> slices;
  slices["A"] = {0, 2};

  NodeTransformFrame frame;
  frame.syncFromNodes(nodes);
  frame.translate("A", 5000.0, 5000.0); // dragged far off-screen

  std::vector<GLint> firsts;
  std::vector<GLsizei> counts;
  TextRenderManager::computeVisibleTextRanges(
      nodes, slices, -10.0, -10.0, 200.0, 200.0, firsts, counts, &frame);
  EXPECT_TRUE(firsts.empty()) << "a node dragged off-screen must be culled";
}

} // namespace noodles
