// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/GraphNodeRenderer.h"

#include <algorithm>

namespace noodles {

namespace {

int getRowSlot(const std::vector<int>& rowSlots, int index) {
  return (index < static_cast<int>(rowSlots.size())) ? rowSlots[index] : index;
}

std::vector<int> getDisplayRowKinds(const NodeData& node) {
  if (node.titleCollapsed) {
    return {};
  }
  if (!node.displayRowKinds.empty()) {
    return node.displayRowKinds;
  }

  int maxSlot = -1;
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    maxSlot = std::max(maxSlot, getRowSlot(node.inputRowSlots, i));
  }
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    maxSlot = std::max(maxSlot, getRowSlot(node.outputRowSlots, i));
  }
  int rowCount = (maxSlot >= 0)
      ? maxSlot + 1
      : std::max(static_cast<int>(node.inputPins.size()), static_cast<int>(node.outputPins.size()));
  std::vector<int> rowKinds(rowCount, 0);
  for (int i = 0; i < static_cast<int>(node.inputRowKinds.size()) && i < rowCount; ++i) {
    if (node.inputRowKinds[i] != 0) {
      int slot = getRowSlot(node.inputRowSlots, i);
      if (slot >= 0 && slot < rowCount) {
        rowKinds[slot] = node.inputRowKinds[i];
      }
    }
  }
  for (int i = 0; i < static_cast<int>(node.outputRowKinds.size()) && i < rowCount; ++i) {
    if (node.outputRowKinds[i] == 0) {
      continue;
    }
    int slot = getRowSlot(node.outputRowSlots, i);
    if (slot >= 0 && slot < rowCount && rowKinds[slot] == 0) {
      rowKinds[slot] = node.outputRowKinds[i];
    }
  }
  return rowKinds;
}

// Node-local helpers for resolvePortPosition (mirror the former Python
// graphView._getPortPosition path; read only NodeData — no graph / USD).
bool isRelationshipPinName(const std::string& name) {
  // Mirrors isRelationshipPin in NodeData.cpp.
  return name == "sources" || name == "affects";
}

bool pinHasVisiblePort(const NodeData& node, const std::string& pinName, bool isOutput) {
  const std::vector<std::string>& pins = isOutput ? node.outputPins : node.inputPins;
  if (std::find(pins.begin(), pins.end(), pinName) != pins.end()) {
    return true;
  }
  const auto& dualSet = isOutput ? node.dualPinNames : node.outputDualPinNames;
  return dualSet.count(pinName) > 0;
}

// The plain port-circle center: edge X (left = input / right = output) + the
// cached per-pin row center Y, with a node-center fallback for unknown pins.
// Shared by getPortPosition and resolvePortPosition so the geometry lives in one
// place.
Vec2d pinCircleCenter(const NodeData& node, const std::string& portName, bool isOutput) {
  const std::vector<std::string>& pins = isOutput ? node.outputPins : node.inputPins;
  const std::vector<double>& centers =
      isOutput ? node.layoutOutputCenterY : node.layoutInputCenterY;
  double portCenterX = isOutput ? node.position[0] + node.size[0] : node.position[0];

  int portIndex = -1;
  for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
    if (pins[i] == portName) {
      portIndex = i;
      break;
    }
  }
  if (portIndex < 0 || portIndex >= static_cast<int>(centers.size())) {
    return Vec2d(portCenterX, node.position[1] + node.size[1] * 0.5);
  }
  return Vec2d(portCenterX, node.position[1] + centers[portIndex]);
}

} // namespace

// --- GraphNodeRenderer base ---

GraphNodeRenderer::GraphNodeRenderer(const RenderConfig& config) : config_(config) {}

double GraphNodeRenderer::getCornerRadius(const NodeData& /*node*/) const {
  return config_.get("nodeCornerRadius", 20.0);
}

double GraphNodeRenderer::getSelectedStrokeWidth() const {
  return config_.get("selectedNodeStrokeWidth", 8.0);
}

double GraphNodeRenderer::getUnselectedStrokeWidth() const {
  return 0.0;
}

std::array<float, 4> GraphNodeRenderer::getStrokeColor(const NodeData& node) const {
  if (node.selected) {
    return config_.selectedNodeStrokeColor;
  }
  return {0.0f, 0.0f, 0.0f, 0.0f};
}

double GraphNodeRenderer::getPortFontSize() const {
  return config_.get("nodePinFontSize", 36.0);
}

double GraphNodeRenderer::getPortMarginH() const {
  return config_.get("nodeMarginH", 32.0);
}

double GraphNodeRenderer::getPortMarginV() const {
  return config_.get("nodeMarginV", 36.0);
}

double GraphNodeRenderer::getPortSpacing() const {
  return config_.get("nodePortSpacing", 2.0);
}

double GraphNodeRenderer::getPortWidth() const {
  return config_.get("nodePortWidth", 32.0) * 0.5;
}

double GraphNodeRenderer::getTitleFontSize() const {
  return config_.get("nodeTitleFontSize", 48.0);
}

double GraphNodeRenderer::getPortTypeFontSize() const {
  return config_.get("nodePinTypeFontSize", 28.0);
}

double GraphNodeRenderer::getSchemaTypeHeight(const NodeData& node, const FontAtlas& fontAtlas)
    const {
  if (!node.schemaTypeName.empty()) {
    return getTitleFontSize() * RenderConfig::kSchemaTypeFontRatio * fontAtlas.lineHeight();
  }
  return 0.0;
}

bool GraphNodeRenderer::pointInRect(const Vec2d& point, const PortHitArea& rect) {
  return point[0] >= rect.minX && point[0] <= rect.maxX && point[1] >= rect.minY &&
      point[1] <= rect.maxY;
}

GraphNodeRenderer::PortHitResult GraphNodeRenderer::getPortAtPoint(
    const NodeData& node,
    const Vec2d& worldPos,
    const FontAtlas& fontAtlas,
    double hitRadiusMultiplier) const {
  PortHitResult closest;
  double closestDistSq = 1e30;
  double portRadius = config_.get("nodePortRadius", 8.0);
  double hitRadius = portRadius * hitRadiusMultiplier;
  double hitRadiusSq = hitRadius * hitRadius;

  for (const auto& portName : node.inputPins) {
    auto portPos = getPortPosition(node, portName, false, fontAtlas);
    double dx = worldPos[0] - portPos[0];
    double dy = worldPos[1] - portPos[1];
    double distSq = dx * dx + dy * dy;
    if (distSq <= hitRadiusSq && distSq < closestDistSq) {
      // Skip unfolded headers (kind 2) — they have no port
      // Folded headers (kind 1) have an aggregate port
      int idx = static_cast<int>(&portName - &node.inputPins[0]);
      if (idx < static_cast<int>(node.inputRowKinds.size())) {
        int kind = node.inputRowKinds[idx];
        if (kind == 2) {
          continue;
        }
      }
      closestDistSq = distSq;
      closest.portName = portName;
      closest.isOutput = false;
      closest.found = true;
    }
  }

  for (const auto& portName : node.outputPins) {
    auto portPos = getPortPosition(node, portName, true, fontAtlas);
    double dx = worldPos[0] - portPos[0];
    double dy = worldPos[1] - portPos[1];
    double distSq = dx * dx + dy * dy;
    if (distSq <= hitRadiusSq && distSq < closestDistSq) {
      // Skip unfolded headers (kind 2) — they have no port
      int idx = static_cast<int>(&portName - &node.outputPins[0]);
      if (idx < static_cast<int>(node.outputRowKinds.size())) {
        int kind = node.outputRowKinds[idx];
        if (kind == 2) {
          continue;
        }
      }
      closestDistSq = distSq;
      closest.portName = portName;
      closest.isOutput = true;
      closest.found = true;
    }
  }

  return closest;
}

GraphNodeRenderer::PortHitResult GraphNodeRenderer::getGroupHeaderAtPoint(
    const NodeData& node,
    const Vec2d& worldPos,
    const FontAtlas& /*fontAtlas*/) const {
  PortHitResult result;

  double portLineHeight = node.layoutPortLineHeight;
  double half = portLineHeight * 0.5;
  bool insideX = worldPos[0] >= node.position[0] && worldPos[0] <= node.position[0] + node.size[0];
  if (!insideX) {
    return result;
  }

  // The per-pin center array is the authoritative row position; the band spans
  // [center - half, center + half].
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    if (i >= static_cast<int>(node.inputRowKinds.size()) ||
        i >= static_cast<int>(node.layoutInputCenterY.size())) {
      break;
    }
    int kind = node.inputRowKinds[i];
    if (kind != 1 && kind != 2) {
      continue;
    }
    double center = node.position[1] + node.layoutInputCenterY[i];
    if (worldPos[1] >= center - half && worldPos[1] <= center + half) {
      result.portName = node.inputPins[i];
      result.isOutput = false;
      result.found = true;
      return result;
    }
  }

  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    if (i >= static_cast<int>(node.outputRowKinds.size()) ||
        i >= static_cast<int>(node.layoutOutputCenterY.size())) {
      break;
    }
    int kind = node.outputRowKinds[i];
    if (kind != 1 && kind != 2) {
      continue;
    }
    double center = node.position[1] + node.layoutOutputCenterY[i];
    if (worldPos[1] >= center - half && worldPos[1] <= center + half) {
      result.portName = node.outputPins[i];
      result.isOutput = true;
      result.found = true;
      return result;
    }
  }

  return result;
}

GraphNodeRenderer::PortHitArea GraphNodeRenderer::getPortHitArea(
    const NodeData& node,
    const std::string& portName,
    bool isOutput,
    const FontAtlas& fontAtlas) const {
  auto portPos = getPortPosition(node, portName, isOutput, fontAtlas);
  double portRadius = config_.get("nodePortRadius", 8.0);
  double fontSize = config_.get("nodeFontSize", 14.0);

  double textWidth = 0.0;
  const auto& glyphs = fontAtlas.glyphs();
  for (unsigned char ch : portName) {
    int unicode = static_cast<int>(ch);
    auto it = glyphs.find(unicode);
    if (it != glyphs.end()) {
      textWidth += it->second.advance;
    }
  }
  textWidth *= fontSize;

  double textHeight = fontSize * (fontAtlas.ascender() - fontAtlas.descender());
  double textMargin = 8.0;

  PortHitArea area{};
  if (isOutput) {
    area.minX = portPos[0] - textWidth - textMargin - portRadius;
    area.maxX = portPos[0] + portRadius;
    area.minY = portPos[1] - std::max(portRadius, textHeight / 2.0);
    area.maxY = portPos[1] + std::max(portRadius, textHeight / 2.0);
  } else {
    area.minX = portPos[0] - portRadius;
    area.maxX = portPos[0] + textWidth + textMargin + portRadius;
    area.minY = portPos[1] - std::max(portRadius, textHeight / 2.0);
    area.maxY = portPos[1] + std::max(portRadius, textHeight / 2.0);
  }
  return area;
}

Vec2d GraphNodeRenderer::getPortPosition(
    const NodeData& node,
    const std::string& portName,
    bool isOutput,
    const FontAtlas& /*fontAtlas*/) const {
  // Port vertical center comes straight from the per-pin layout the producer
  // resolved (node.layoutInputCenterY / layoutOutputCenterY). No slot lookup or
  // row-step math here — that is what keeps ports aligned with the row stripes.
  return pinCircleCenter(node, portName, isOutput);
}

Vec2d GraphNodeRenderer::resolvePortPosition(
    const NodeData& node,
    const std::string& portName,
    bool isOutput) const {
  // Synthetic relationship port: a relationship pin (sources/affects) with no
  // visible row gets a title-area aggregate handle. Checked first, like the
  // former Python _getPortPosition.
  if (isRelationshipPinName(portName) && !pinHasVisiblePort(node, portName, isOutput)) {
    double titleHeight =
        node.layoutTitleHeight > 0.0 ? node.layoutTitleHeight : std::min(node.size[1], 32.0);
    double centerY = node.position[1] + titleHeight * 0.5;
    double x = isOutput ? node.position[0] + node.size[0] : node.position[0];
    return Vec2d(x, centerY);
  }
  // Dual pin: an input-side dual pin also exposes an output port mirrored to the
  // right edge across the node's vertical center line.
  if (isOutput && node.dualPinNames.count(portName) > 0) {
    Vec2d inPos = pinCircleCenter(node, portName, false);
    double rightX = node.position[0] + node.size[0] - (inPos[0] - node.position[0]);
    return Vec2d(rightX, inPos[1]);
  }
  // Output-dual pin: symmetric mirror of the output port onto the left edge.
  if (!isOutput && node.outputDualPinNames.count(portName) > 0) {
    Vec2d outPos = pinCircleCenter(node, portName, true);
    double leftX = node.position[0] + node.size[0] - (outPos[0] - node.position[0]);
    return Vec2d(leftX, outPos[1]);
  }
  return pinCircleCenter(node, portName, isOutput);
}

// --- DefaultNodeRenderer ---

DefaultNodeRenderer::DefaultNodeRenderer(const RenderConfig& config) : GraphNodeRenderer(config) {}

std::vector<NodeVertex>
DefaultNodeRenderer::generateVertices(NodeData& node, float depth, const FontAtlas& /*fontAtlas*/) {
  int nodeBgHigh = config_.getInt("nodeBgHigh", 90);
  int nodeBgLow = config_.getInt("nodeBgLow", 86);
  int nodeBgAlpha = config_.getInt("nodeBgAlpha", 230);
  double nodeShadowFactor = config_.get("nodeShadowFactor", 0.9);
  double nodeTypeSaturation = config_.get("nodeTypeSaturation", 0.35);
  double nodeTypeBrightness = config_.get("nodeTypeBrightness", 0.5);

  double titleHeight = node.layoutTitleHeight;

  float innerStroke =
      node.selected ? static_cast<float>(config_.get("selectedNodeStrokeWidth", 4.0)) : 0.0f;

  double shadow = nodeShadowFactor;
  int bgHighShadow = static_cast<int>(nodeBgHigh * shadow);
  int bgLowShadow = static_cast<int>(nodeBgLow * shadow);

  double selectedBrightness = 1.3;
  int selectedBgHigh = std::min(255, static_cast<int>(nodeBgHigh * selectedBrightness));
  int selectedBgLow = std::min(255, static_cast<int>(nodeBgLow * selectedBrightness));
  int selectedBgHighShadow = std::min(255, static_cast<int>(bgHighShadow * selectedBrightness));
  int selectedBgLowShadow = std::min(255, static_cast<int>(bgLowShadow * selectedBrightness));

  int red, grn, blu;
  if (node.displayColorR >= 0.0f) {
    red = static_cast<int>(node.displayColorR * 255.0f);
    grn = static_cast<int>(node.displayColorG * 255.0f);
    blu = static_cast<int>(node.displayColorB * 255.0f);
  } else {
    size_t hashVal = std::hash<std::string>{}(node.type) % 360;
    auto hue = static_cast<float>(hashVal);
    auto rgb = VertexGenerator::hsvToRgb(
        hue, static_cast<float>(nodeTypeSaturation), static_cast<float>(nodeTypeBrightness));
    red = static_cast<int>(rgb[0] * 255.0f);
    grn = static_cast<int>(rgb[1] * 255.0f);
    blu = static_cast<int>(rgb[2] * 255.0f);
  }

  int redShadow = static_cast<int>(red * shadow);
  int grnShadow = static_cast<int>(grn * shadow);
  int bluShadow = static_cast<int>(blu * shadow);

  int selectedRed = std::min(255, static_cast<int>(red * selectedBrightness));
  int selectedGrn = std::min(255, static_cast<int>(grn * selectedBrightness));
  int selectedBlu = std::min(255, static_cast<int>(blu * selectedBrightness));
  int selectedRedShadow = std::min(255, static_cast<int>(redShadow * selectedBrightness));
  int selectedGrnShadow = std::min(255, static_cast<int>(grnShadow * selectedBrightness));
  int selectedBluShadow = std::min(255, static_cast<int>(bluShadow * selectedBrightness));

  float selectedFlag = node.selected ? 1.0f : 0.0f;

  DefaultNodeColors colors{
      nodeBgHigh,
      nodeBgLow,
      nodeBgAlpha,
      bgHighShadow,
      bgLowShadow,
      selectedBgHigh,
      selectedBgLow,
      selectedBgHighShadow,
      selectedBgLowShadow,
      red,
      grn,
      blu,
      redShadow,
      grnShadow,
      bluShadow,
      selectedRed,
      selectedGrn,
      selectedBlu,
      selectedRedShadow,
      selectedGrnShadow,
      selectedBluShadow};

  // Read layout metrics from the cached values set by calculateNodeSize.
  double portLineHeight = node.layoutPortLineHeight;
  double portStartY = node.layoutPortStartY;
  std::vector<int> mergedRowKinds = getDisplayRowKinds(node);
  int pinCount = node.layoutRowCount;

  return VertexGenerator::generateDefaultNodeVertices(
      Vec2d(node.position[0], node.position[1]),
      Vec2d(node.size[0], node.size[1]),
      depth,
      static_cast<float>(titleHeight),
      colors,
      innerStroke,
      selectedFlag,
      pinCount,
      static_cast<float>(portLineHeight),
      static_cast<float>(portStartY),
      mergedRowKinds);
}

// --- GraffiNodeRenderer ---

GraffiNodeRenderer::GraffiNodeRenderer(const RenderConfig& config) : GraphNodeRenderer(config) {}

std::vector<NodeVertex>
GraffiNodeRenderer::generateVertices(NodeData& node, float depth, const FontAtlas& /*fontAtlas*/) {
  double nodePortWidth = getPortWidth();

  int bodyR = 13, bodyG = 18, bodyB = 23, bodyAlpha = 255;
  int headerR = 0, headerG = 0, headerB = 0;
  int portR = 49, portG = 115, portB = 100, portAlpha = 255;

  float strokeWidth = 0.0f;
  int headerAlpha = 0;
  if (node.selected) {
    strokeWidth = static_cast<float>(config_.get("selectedNodeStrokeWidth", 8.0));
    headerAlpha = 100;
  } else {
    strokeWidth = static_cast<float>(getUnselectedStrokeWidth());
    headerAlpha = 60;
  }

  double selectedBrightness = 1.3;
  int selectedBodyR = std::min(255, static_cast<int>(bodyR * selectedBrightness));
  int selectedBodyG = std::min(255, static_cast<int>(bodyG * selectedBrightness));
  int selectedBodyB = std::min(255, static_cast<int>(bodyB * selectedBrightness));
  int selectedHeaderR = std::min(255, static_cast<int>(headerR * selectedBrightness));
  int selectedHeaderG = std::min(255, static_cast<int>(headerG * selectedBrightness));
  int selectedHeaderB = std::min(255, static_cast<int>(headerB * selectedBrightness));
  float selectedFlag = node.selected ? 1.0f : 0.0f;

  // Port vertical centers come from the per-pin layout (node.layoutInput/OutputCenterY).
  auto portRadius = static_cast<float>(nodePortWidth * 0.5);

  std::vector<PortInfo> inputPorts;
  if (!node.titleCollapsed) {
    for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
      // Skip unfolded headers (kind 2) — folded headers (kind 1) have aggregate port
      if (i < static_cast<int>(node.inputRowKinds.size())) {
        int kind = node.inputRowKinds[i];
        if (kind == 2) {
          continue;
        }
      }
      PortInfo port;
      double centerY = (i < static_cast<int>(node.layoutInputCenterY.size()))
          ? node.position[1] + node.layoutInputCenterY[i]
          : node.position[1] + node.size[1] * 0.5;
      port.position = Vec2d(node.position[0], centerY);
      port.r = static_cast<uint8_t>(portR);
      port.g = static_cast<uint8_t>(portG);
      port.b = static_cast<uint8_t>(portB);
      port.a = static_cast<uint8_t>(portAlpha);
      inputPorts.push_back(port);
    }
  }

  std::vector<PortInfo> outputPorts;
  double x1 = node.position[0] + node.size[0];
  if (!node.titleCollapsed) {
    for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
      // Skip unfolded headers (kind 2) — folded headers (kind 1) have aggregate port
      if (i < static_cast<int>(node.outputRowKinds.size())) {
        int kind = node.outputRowKinds[i];
        if (kind == 2) {
          continue;
        }
      }
      PortInfo port;
      double centerY = (i < static_cast<int>(node.layoutOutputCenterY.size()))
          ? node.position[1] + node.layoutOutputCenterY[i]
          : node.position[1] + node.size[1] * 0.5;
      port.position = Vec2d(x1, centerY);
      port.r = static_cast<uint8_t>(portR);
      port.g = static_cast<uint8_t>(portG);
      port.b = static_cast<uint8_t>(portB);
      port.a = static_cast<uint8_t>(portAlpha);
      outputPorts.push_back(port);
    }
  }

  GraffiNodeColors colors{
      bodyR,
      bodyG,
      bodyB,
      bodyAlpha,
      headerR,
      headerG,
      headerB,
      headerAlpha,
      selectedBodyR,
      selectedBodyG,
      selectedBodyB,
      selectedHeaderR,
      selectedHeaderG,
      selectedHeaderB};

  return VertexGenerator::generateGraffiNodeVertices(
      Vec2d(node.position[0], node.position[1]),
      Vec2d(node.size[0], node.size[1]),
      depth,
      static_cast<float>(node.layoutTitleHeight),
      colors,
      inputPorts,
      outputPorts,
      portRadius,
      strokeWidth,
      selectedFlag);
}

std::array<float, 4> GraffiNodeRenderer::getStrokeColor(const NodeData& node) const {
  if (node.selected) {
    return {1.0f, 1.0f, 0.117f, 1.0f};
  }
  return {85.0f / 255.0f, 85.0f / 255.0f, 85.0f / 255.0f, 1.0f};
}

double GraffiNodeRenderer::getUnselectedStrokeWidth() const {
  return 4.0;
}

double GraffiNodeRenderer::getPortFontSize() const {
  return 16.0;
}

double GraffiNodeRenderer::getPortMarginH() const {
  return 32.0;
}

double GraffiNodeRenderer::getPortMarginV() const {
  return 12.0;
}

double GraffiNodeRenderer::getPortSpacing() const {
  return 8.0;
}

double GraffiNodeRenderer::getTitleFontSize() const {
  return 20.0;
}

double GraffiNodeRenderer::getPortTypeFontSize() const {
  return 12.0;
}

} // namespace noodles
