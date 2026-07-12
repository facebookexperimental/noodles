// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/VertexGenerator.h"

namespace noodles {

std::vector<NodeVertex> VertexGenerator::generateDefaultNodeVertices(
    const Vec2d& nodePos,
    const Vec2d& nodeSize,
    float depth,
    float titleHeight,
    const DefaultNodeColors& c,
    float innerStroke,
    float selectedFlag,
    int pinCount,
    float portLineHeight,
    float portStartY,
    const std::vector<int>& rowKinds) {
  std::vector<NodeVertex> vertices;
  size_t rowQuads = pinCount > 0 ? static_cast<size_t>(pinCount) * 6 : 0;
  size_t spacerQuads = pinCount > 1 ? static_cast<size_t>(pinCount - 1) * 6 : 0;
  vertices.reserve(12 + rowQuads + spacerQuads + (innerStroke > 0.0f ? 6 : 0));

  float w = nodeSize[0];
  float h = nodeSize[1];
  float x0 = nodePos[0];
  float y0 = nodePos[1];
  float x1 = x0 + w;
  float y1 = y0 + h;

  // Background rectangle (6 vertices = 2 triangles).
  // The bg/title quads no longer carry innerStroke — the selection ring is drawn
  // by a dedicated stroke-overlay quad on top (center-stroke, see below).
  vertices.emplace_back(
      x0,
      y0,
      depth,
      0,
      0,
      w,
      h,
      c.bgHigh,
      c.bgLow,
      c.bgLow,
      c.bgAlpha,
      0.0f,
      c.selectedBgHigh,
      c.selectedBgLow,
      c.selectedBgLow,
      c.bgAlpha,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y0,
      depth,
      w,
      0,
      w,
      h,
      c.bgHigh,
      c.bgLow,
      c.bgHigh,
      c.bgAlpha,
      0.0f,
      c.selectedBgHigh,
      c.selectedBgLow,
      c.selectedBgHigh,
      c.bgAlpha,
      selectedFlag);
  vertices.emplace_back(
      x0,
      y1,
      depth,
      0,
      h,
      w,
      h,
      c.bgLowShadow,
      c.bgHighShadow,
      c.bgLowShadow,
      c.bgAlpha,
      0.0f,
      c.selectedBgLowShadow,
      c.selectedBgHighShadow,
      c.selectedBgLowShadow,
      c.bgAlpha,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y0,
      depth,
      w,
      0,
      w,
      h,
      c.bgHigh,
      c.bgLow,
      c.bgHigh,
      c.bgAlpha,
      0.0f,
      c.selectedBgHigh,
      c.selectedBgLow,
      c.selectedBgHigh,
      c.bgAlpha,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y1,
      depth,
      w,
      h,
      w,
      h,
      c.bgLowShadow,
      c.bgLowShadow,
      c.bgHighShadow,
      c.bgAlpha,
      0.0f,
      c.selectedBgLowShadow,
      c.selectedBgLowShadow,
      c.selectedBgHighShadow,
      c.bgAlpha,
      selectedFlag);
  vertices.emplace_back(
      x0,
      y1,
      depth,
      0,
      h,
      w,
      h,
      c.bgLowShadow,
      c.bgHighShadow,
      c.bgLowShadow,
      c.bgAlpha,
      0.0f,
      c.selectedBgLowShadow,
      c.selectedBgHighShadow,
      c.selectedBgLowShadow,
      c.bgAlpha,
      selectedFlag);

  // Title bar rectangle (6 vertices = 2 triangles)
  float y1Title = y0 + titleHeight;

  vertices.emplace_back(
      x0,
      y0,
      depth,
      0,
      0,
      w,
      h,
      c.titleR,
      c.titleG,
      c.titleB,
      255,
      0.0f,
      c.selectedTitleR,
      c.selectedTitleG,
      c.selectedTitleB,
      255,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y0,
      depth,
      w,
      0,
      w,
      h,
      c.titleR,
      c.titleG,
      c.titleB,
      255,
      0.0f,
      c.selectedTitleR,
      c.selectedTitleG,
      c.selectedTitleB,
      255,
      selectedFlag);
  vertices.emplace_back(
      x0,
      y1Title,
      depth,
      0,
      titleHeight,
      w,
      h,
      c.titleRShadow,
      c.titleGShadow,
      c.titleBShadow,
      255,
      0.0f,
      c.selectedTitleRShadow,
      c.selectedTitleGShadow,
      c.selectedTitleBShadow,
      255,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y0,
      depth,
      w,
      0,
      w,
      h,
      c.titleR,
      c.titleG,
      c.titleB,
      255,
      0.0f,
      c.selectedTitleR,
      c.selectedTitleG,
      c.selectedTitleB,
      255,
      selectedFlag);
  vertices.emplace_back(
      x1,
      y1Title,
      depth,
      w,
      titleHeight,
      w,
      h,
      c.titleRShadow,
      c.titleGShadow,
      c.titleBShadow,
      255,
      0.0f,
      c.selectedTitleRShadow,
      c.selectedTitleGShadow,
      c.selectedTitleBShadow,
      255,
      selectedFlag);
  vertices.emplace_back(
      x0,
      y1Title,
      depth,
      0,
      titleHeight,
      w,
      h,
      c.titleRShadow,
      c.titleGShadow,
      c.titleBShadow,
      255,
      0.0f,
      c.selectedTitleRShadow,
      c.selectedTitleGShadow,
      c.selectedTitleBShadow,
      255,
      selectedFlag);

  // Per-pin row stripes and spacers
  if (pinCount > 0 && portLineHeight > 0.0f) {
    constexpr float kStripeDepthOffset = 0.00003f;
    constexpr float kSpacerDepthOffset = 0.00006f;
    float stripeDepth = depth + kStripeDepthOffset;
    float spacerDepth = depth + kSpacerDepthOffset;

    int altRowCount = 0;
    for (int i = 0; i < pinCount; ++i) {
      float rowY = y0 + portStartY + i * portLineHeight;
      float rowY1 = rowY + portLineHeight;

      int rowKind = (!rowKinds.empty() && i < static_cast<int>(rowKinds.size())) ? rowKinds[i] : 0;
      int rowGrey = 0;
      int selectedRowGrey = 0;
      int rowAlpha = 0;
      if (rowKind == 1 || rowKind == 2) {
        // Group header: brighter white overlay to convey hierarchy (no alternation).
        rowGrey = 255;
        selectedRowGrey = 255;
        rowAlpha = 40;
      } else {
        // Normal pin (0) or child pin (3): very subtle alternation.
        // Every other row gets a faint white overlay; the rest are invisible.
        if (altRowCount % 2 != 0) {
          rowGrey = 255;
          selectedRowGrey = 255;
          rowAlpha = 8;
        } else {
          rowGrey = 0;
          selectedRowGrey = 0;
          rowAlpha = 0;
        }
        altRowCount++;
      }

      // Row stripe quad (uses full node w,h for SDF rounding)
      float ru0 = 0;
      float rv0 = rowY - y0;
      float ru1 = w;
      float rv1 = rowY1 - y0;

      vertices.emplace_back(
          x0,
          rowY,
          stripeDepth,
          ru0,
          rv0,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);
      vertices.emplace_back(
          x1,
          rowY,
          stripeDepth,
          ru1,
          rv0,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);
      vertices.emplace_back(
          x0,
          rowY1,
          stripeDepth,
          ru0,
          rv1,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);
      vertices.emplace_back(
          x1,
          rowY,
          stripeDepth,
          ru1,
          rv0,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);
      vertices.emplace_back(
          x1,
          rowY1,
          stripeDepth,
          ru1,
          rv1,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);
      vertices.emplace_back(
          x0,
          rowY1,
          stripeDepth,
          ru0,
          rv1,
          w,
          h,
          rowGrey,
          rowGrey,
          rowGrey,
          rowAlpha,
          0.0f,
          selectedRowGrey,
          selectedRowGrey,
          selectedRowGrey,
          rowAlpha,
          selectedFlag);

      // 1-unit spacer between rows (not after last row)
      if (i < pinCount - 1) {
        float spacerY0 = rowY1 - 1.0f;
        float spacerY1 = rowY1;
        float sv0 = spacerY0 - y0;
        float sv1 = spacerY1 - y0;

        vertices.emplace_back(
            x0,
            spacerY0,
            spacerDepth,
            0,
            sv0,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
        vertices.emplace_back(
            x1,
            spacerY0,
            spacerDepth,
            w,
            sv0,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
        vertices.emplace_back(
            x0,
            spacerY1,
            spacerDepth,
            0,
            sv1,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
        vertices.emplace_back(
            x1,
            spacerY0,
            spacerDepth,
            w,
            sv0,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
        vertices.emplace_back(
            x1,
            spacerY1,
            spacerDepth,
            w,
            sv1,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
        vertices.emplace_back(
            x0,
            spacerY1,
            spacerDepth,
            0,
            sv1,
            w,
            h,
            0,
            0,
            0,
            180,
            0.0f,
            0,
            0,
            0,
            180,
            selectedFlag);
      }
    }
  }

  // Selection stroke overlay: one ring-only quad drawn on top of all rows so
  // the inner-stroke highlight is never occluded by row/spacer fills. Uses a
  // negative innerStroke (sentinel) that the fragment shader interprets as
  // "draw only the stroke ring, discard the interior."
  if (innerStroke > 0.0f) {
    constexpr float kOverlayDepthOffset = 0.00009f;
    float overlayDepth = depth + kOverlayDepthOffset;
    float negStroke = -innerStroke;
    // Center-stroke: the ring extends halfStroke outside the node edge, so the
    // overlay quad must be larger than the node to have fragments there.
    float hs = innerStroke * 0.5f;
    float ox0 = x0 - hs, oy0 = y0 - hs;
    float ox1 = x1 + hs, oy1 = y1 + hs;
    // Local coords are relative to the original node rect (the SDF rounds that,
    // not the expanded quad); negative coords = outside the node edge.
    vertices.emplace_back(
        ox0, oy0, overlayDepth, -hs, -hs, w, h, 0, 0, 0, 0, negStroke, 0, 0, 0, 0, selectedFlag);
    vertices.emplace_back(
        ox1, oy0, overlayDepth, w + hs, -hs, w, h, 0, 0, 0, 0, negStroke, 0, 0, 0, 0, selectedFlag);
    vertices.emplace_back(
        ox0, oy1, overlayDepth, -hs, h + hs, w, h, 0, 0, 0, 0, negStroke, 0, 0, 0, 0, selectedFlag);
    vertices.emplace_back(
        ox1, oy0, overlayDepth, w + hs, -hs, w, h, 0, 0, 0, 0, negStroke, 0, 0, 0, 0, selectedFlag);
    vertices.emplace_back(
        ox1,
        oy1,
        overlayDepth,
        w + hs,
        h + hs,
        w,
        h,
        0,
        0,
        0,
        0,
        negStroke,
        0,
        0,
        0,
        0,
        selectedFlag);
    vertices.emplace_back(
        ox0, oy1, overlayDepth, -hs, h + hs, w, h, 0, 0, 0, 0, negStroke, 0, 0, 0, 0, selectedFlag);
  }

  return vertices;
}

Vec4f VertexGenerator::hsvToRgb(float h, float s, float v, float a) {
  float ch = v * s;
  float x = ch * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
  float m = v - ch;

  float r = 0, g = 0, b = 0;

  if (h < 60.0f) {
    r = ch;
    g = x;
    b = 0.0f;
  } else if (h < 120.0f) {
    r = x;
    g = ch;
    b = 0.0f;
  } else if (h < 180.0f) {
    r = 0.0f;
    g = ch;
    b = x;
  } else if (h < 240.0f) {
    r = 0.0f;
    g = x;
    b = ch;
  } else if (h < 300.0f) {
    r = x;
    g = 0.0f;
    b = ch;
  } else {
    r = ch;
    g = 0.0f;
    b = x;
  }

  return Vec4f(r + m, g + m, b + m, a);
}

std::vector<NodeVertex> VertexGenerator::generateGraffiNodeVertices(
    const Vec2d& nodePos,
    const Vec2d& nodeSize,
    float depth,
    float titleHeight,
    const GraffiNodeColors& c,
    const std::vector<PortInfo>& inputPorts,
    const std::vector<PortInfo>& outputPorts,
    float portRadius,
    float bodyStroke,
    float selectedFlag) {
  float w = nodeSize[0];
  float h = nodeSize[1];
  float x0 = nodePos[0];
  float y0 = nodePos[1];
  float x1 = x0 + w;
  float y1 = y0 + h;
  bool isSelected = selectedFlag > 0.0f;

  size_t totalPorts = inputPorts.size() + outputPorts.size();
  size_t count = 12 + (isSelected ? 6 : 0) + 6 * totalPorts;
  std::vector<NodeVertex> vertices;
  vertices.reserve(count);

  auto addQuad = [&](float qx0,
                     float qy0,
                     float qx1,
                     float qy1,
                     float z,
                     float qu0,
                     float qv0,
                     float qu1,
                     float qv1,
                     float qw,
                     float qh,
                     int r,
                     int g,
                     int b,
                     int a,
                     float stroke,
                     int sr,
                     int sg,
                     int sb,
                     int sa,
                     float sel) {
    vertices.emplace_back(qx0, qy0, z, qu0, qv0, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
    vertices.emplace_back(qx1, qy0, z, qu1, qv0, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
    vertices.emplace_back(qx0, qy1, z, qu0, qv1, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
    vertices.emplace_back(qx1, qy0, z, qu1, qv0, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
    vertices.emplace_back(qx1, qy1, z, qu1, qv1, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
    vertices.emplace_back(qx0, qy1, z, qu0, qv1, qw, qh, r, g, b, a, stroke, sr, sg, sb, sa, sel);
  };

  addQuad(
      x0,
      y0,
      x1,
      y1,
      depth,
      0,
      0,
      w,
      h,
      w,
      h,
      c.bodyR,
      c.bodyG,
      c.bodyB,
      c.bodyAlpha,
      bodyStroke,
      c.selectedBodyR,
      c.selectedBodyG,
      c.selectedBodyB,
      c.bodyAlpha,
      selectedFlag);

  if (isSelected) {
    addQuad(
        x0,
        y0,
        x1,
        y1,
        depth + 0.05f,
        0,
        0,
        w,
        h,
        w,
        h,
        255,
        255,
        255,
        40,
        0.0f,
        255,
        255,
        255,
        40,
        selectedFlag);
  }

  float y1Title = y0 + titleHeight;
  addQuad(
      x0,
      y0,
      x1,
      y1Title,
      depth + 0.1f,
      0,
      0,
      w,
      titleHeight,
      w,
      h,
      c.headerR,
      c.headerG,
      c.headerB,
      c.headerAlpha,
      0.0f,
      c.selectedHeaderR,
      c.selectedHeaderG,
      c.selectedHeaderB,
      c.headerAlpha,
      selectedFlag);

  float portDiam = portRadius * 2.0f;
  auto addPortQuad = [&](const PortInfo& port) {
    auto cx = static_cast<float>(port.position[0]);
    auto cy = static_cast<float>(port.position[1]);
    float px0 = cx - portRadius;
    float py0 = cy - portRadius;
    float px1 = cx + portRadius;
    float py1 = cy + portRadius;
    addQuad(
        px0,
        py0,
        px1,
        py1,
        depth + 0.2f,
        0,
        0,
        portDiam,
        portDiam,
        portDiam,
        portDiam,
        port.r,
        port.g,
        port.b,
        port.a,
        0.0f,
        port.r,
        port.g,
        port.b,
        port.a,
        selectedFlag);
  };

  for (const auto& port : inputPorts) {
    addPortQuad(port);
  }
  for (const auto& port : outputPorts) {
    addPortQuad(port);
  }

  return vertices;
}

} // namespace noodles
