// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/GraphNodeRenderer.h"

#include <algorithm>

namespace noodles {

// --- GraphNodeRenderer base ---

GraphNodeRenderer::GraphNodeRenderer(const RenderConfig& config) : config_(config) {}

double GraphNodeRenderer::getGlobalScale() const {
  return config_.get("globalNodeScale", 2.0);
}

double GraphNodeRenderer::getCornerRadius(const NodeData& /*node*/) const {
  double scale = getGlobalScale();
  return config_.get("nodeCornerRadius", 10.0) * scale;
}

double GraphNodeRenderer::getSelectedStrokeWidth() const {
  double scale = getGlobalScale();
  return config_.get("selectedNodeStrokeWidth", 4.0) * scale;
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

bool GraphNodeRenderer::getOutputPortsTopToBottom() const {
  return false;
}

double GraphNodeRenderer::getPortFontSize() const {
  double scale = getGlobalScale();
  return config_.get("nodePinFontSize", 18.0) * scale;
}

double GraphNodeRenderer::getPortMarginH() const {
  double scale = getGlobalScale();
  return config_.get("nodeMarginH", 16.0) * scale;
}

double GraphNodeRenderer::getPortMarginV() const {
  double scale = getGlobalScale();
  return config_.get("nodeMarginV", 18.0) * scale;
}

double GraphNodeRenderer::getPortSpacing() const {
  double scale = getGlobalScale();
  return config_.get("nodePortSpacing", 1.0) * scale;
}

double GraphNodeRenderer::getPortWidth() const {
  double scale = getGlobalScale();
  return config_.get("nodePortWidth", 16.0) * scale * 0.5;
}

double GraphNodeRenderer::getTitleFontSize() const {
  double scale = getGlobalScale();
  return config_.get("nodeTitleFontSize", 24.0) * scale;
}

double GraphNodeRenderer::getPortTypeFontSize() const {
  double scale = getGlobalScale();
  return config_.get("nodePinTypeFontSize", 14.0) * scale;
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
    const FontAtlas& fontAtlas) const {
  PortHitResult result;

  double nodeTitleFontSize = getTitleFontSize();
  double nodePinFontSize = getPortFontSize();
  double nodePinTypeFontSize = getPortTypeFontSize();
  double nodeMarginV = getPortMarginV();
  double nodePortSpacing = getPortSpacing();

  double titleHeight =
      nodeTitleFontSize * (fontAtlas.ascender() - fontAtlas.descender()) + nodeMarginV * 2.0;

  double portNameHeight = nodePinFontSize * fontAtlas.lineHeight();
  double portTypeHeight = nodePinTypeFontSize * fontAtlas.lineHeight();
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;

  double portStartY = node.position[1] + titleHeight + nodeMarginV;

  // Check input pins for group headers
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    if (i >= static_cast<int>(node.inputRowKinds.size())) {
      break;
    }
    int kind = node.inputRowKinds[i];
    if (kind != 1 && kind != 2) {
      continue;
    }
    double rowY = portStartY + i * portLineHeight;
    double rowY1 = rowY + portLineHeight;
    if (worldPos[1] >= rowY && worldPos[1] <= rowY1 && worldPos[0] >= node.position[0] &&
        worldPos[0] <= node.position[0] + node.size[0]) {
      result.portName = node.inputPins[i];
      result.isOutput = false;
      result.found = true;
      return result;
    }
  }

  // Check output pins for group headers
  double outputPortStartY =
      portStartY + static_cast<double>(node.inputPins.size()) * portLineHeight;
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    if (i >= static_cast<int>(node.outputRowKinds.size())) {
      break;
    }
    int kind = node.outputRowKinds[i];
    if (kind != 1 && kind != 2) {
      continue;
    }
    double rowY = outputPortStartY + i * portLineHeight;
    double rowY1 = rowY + portLineHeight;
    if (worldPos[1] >= rowY && worldPos[1] <= rowY1 && worldPos[0] >= node.position[0] &&
        worldPos[0] <= node.position[0] + node.size[0]) {
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
    const FontAtlas& fontAtlas) const {
  double nodeTitleFontSize = getTitleFontSize();
  double nodePinFontSize = getPortFontSize();
  double nodePinTypeFontSize = getPortTypeFontSize();
  double nodeMarginV = getPortMarginV();
  double nodePortSpacing = getPortSpacing();
  bool outputPortsTopToBottom = getOutputPortsTopToBottom();

  double titleHeight =
      nodeTitleFontSize * (fontAtlas.ascender() - fontAtlas.descender()) + nodeMarginV * 2.0;

  double portNameHeight = nodePinFontSize * fontAtlas.lineHeight();
  double portTypeHeight = nodePinTypeFontSize * fontAtlas.lineHeight();
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;

  double portStartY = node.position[1] + titleHeight + nodeMarginV;

  // Collapsed nodes: all ports converge to a single aggregate position
  if (node.titleCollapsed) {
    double aggregateY = node.position[1] + node.size[1] * 0.5;
    if (isOutput) {
      return Vec2d(node.position[0] + node.size[0], aggregateY);
    } else {
      return Vec2d(node.position[0], aggregateY);
    }
  }

  if (isOutput) {
    int portIndex = -1;
    for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
      if (node.outputPins[i] == portName) {
        portIndex = i;
        break;
      }
    }

    if (portIndex < 0) {
      return Vec2d(node.position[0] + node.size[0], node.position[1] + node.size[1] * 0.5);
    }

    double outputPortStartY = 0.0;
    if (outputPortsTopToBottom) {
      outputPortStartY = portStartY;
    } else {
      outputPortStartY = portStartY + static_cast<double>(node.inputPins.size()) * portLineHeight;
    }

    // Folded headers (kind 1) center in full row; normal/child rows center in portName area
    double verticalCenter = portNameHeight * 0.5;
    if (portIndex < static_cast<int>(node.outputRowKinds.size()) &&
        node.outputRowKinds[portIndex] == 1) {
      verticalCenter = portLineHeight * 0.5;
    }
    double portCenterY = outputPortStartY + portIndex * portLineHeight + verticalCenter;
    double portCenterX = node.position[0] + node.size[0];
    return Vec2d(portCenterX, portCenterY);
  } else {
    int portIndex = -1;
    for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
      if (node.inputPins[i] == portName) {
        portIndex = i;
        break;
      }
    }

    if (portIndex < 0) {
      return Vec2d(node.position[0], node.position[1] + node.size[1] * 0.5);
    }

    // Folded headers (kind 1) center in full row; normal/child rows center in portName area
    double verticalCenter = portNameHeight * 0.5;
    if (portIndex < static_cast<int>(node.inputRowKinds.size()) &&
        node.inputRowKinds[portIndex] == 1) {
      verticalCenter = portLineHeight * 0.5;
    }
    double portCenterY = portStartY + portIndex * portLineHeight + verticalCenter;
    double portCenterX = node.position[0];
    return Vec2d(portCenterX, portCenterY);
  }
}

// --- DefaultNodeRenderer ---

DefaultNodeRenderer::DefaultNodeRenderer(const RenderConfig& config) : GraphNodeRenderer(config) {}

std::vector<NodeVertex>
DefaultNodeRenderer::generateVertices(NodeData& node, float depth, const FontAtlas& fontAtlas) {
  double nodeTitleFontSize = getTitleFontSize();
  double nodeMarginV = getPortMarginV();
  int nodeBgHigh = config_.getInt("nodeBgHigh", 50);
  int nodeBgLow = config_.getInt("nodeBgLow", 40);
  int nodeBgAlpha = config_.getInt("nodeBgAlpha", 248);
  double nodeShadowFactor = config_.get("nodeShadowFactor", 0.6);
  double nodeTypeSaturation = config_.get("nodeTypeSaturation", 0.6);
  double nodeTypeBrightness = config_.get("nodeTypeBrightness", 0.6);

  double titleHeight =
      nodeTitleFontSize * (fontAtlas.ascender() - fontAtlas.descender()) + nodeMarginV * 2.0;

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

  double nodePinFontSize = getPortFontSize();
  double nodePinTypeFontSize = getPortTypeFontSize();
  double nodePortSpacing = getPortSpacing();

  double portNameHeight = nodePinFontSize * fontAtlas.lineHeight();
  double portTypeHeight = nodePinTypeFontSize * fontAtlas.lineHeight();
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;
  double portStartY = titleHeight + nodeMarginV;
  bool outputPortsTopToBottom = getOutputPortsTopToBottom();
  int inputCount = static_cast<int>(node.inputPins.size());
  int outputCount = static_cast<int>(node.outputPins.size());

  // Build merged rowKinds for the visible rows
  std::vector<int> mergedRowKinds;
  int pinCount;

  if (outputPortsTopToBottom) {
    // Inputs and outputs share rows — use max count, merge kinds
    pinCount = std::max(inputCount, outputCount);
    mergedRowKinds.resize(pinCount, 0);
    for (int i = 0; i < pinCount; ++i) {
      int inKind = (i < static_cast<int>(node.inputRowKinds.size())) ? node.inputRowKinds[i] : 0;
      int outKind = (i < static_cast<int>(node.outputRowKinds.size())) ? node.outputRowKinds[i] : 0;
      // Use whichever side has a group kind (prefer input side)
      if (inKind != 0) {
        mergedRowKinds[i] = inKind;
      } else if (outKind != 0) {
        mergedRowKinds[i] = outKind;
      }
    }
  } else {
    // Inputs then outputs stacked — pad each side to its actual pin count
    // so output kinds start at the correct row offset even when
    // inputRowKinds is empty (no groups on the input side).
    pinCount = inputCount + outputCount;
    mergedRowKinds.resize(pinCount, 0);
    for (int i = 0; i < static_cast<int>(node.inputRowKinds.size()) && i < inputCount; ++i) {
      mergedRowKinds[i] = node.inputRowKinds[i];
    }
    for (int i = 0; i < static_cast<int>(node.outputRowKinds.size()) && i < outputCount; ++i) {
      mergedRowKinds[inputCount + i] = node.outputRowKinds[i];
    }
  }

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
GraffiNodeRenderer::generateVertices(NodeData& node, float depth, const FontAtlas& fontAtlas) {
  double nodeTitleFontSize = getTitleFontSize();
  double nodePinFontSize = getPortFontSize();
  double nodePinTypeFontSize = getPortTypeFontSize();
  double nodeMarginV = getPortMarginV();
  double nodePortWidth = getPortWidth();
  double nodePortSpacing = getPortSpacing();
  bool outputPortsTopToBottom = getOutputPortsTopToBottom();
  double scale = getGlobalScale();

  int bodyR = 13, bodyG = 18, bodyB = 23, bodyAlpha = 255;
  int headerR = 0, headerG = 0, headerB = 0;
  int portR = 49, portG = 115, portB = 100, portAlpha = 255;

  float strokeWidth = 0.0f;
  int headerAlpha = 0;
  if (node.selected) {
    strokeWidth = static_cast<float>(config_.get("selectedNodeStrokeWidth", 4.0) * scale);
    headerAlpha = 100;
  } else {
    strokeWidth = static_cast<float>(2.0 * scale);
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

  double titleHeight =
      nodeTitleFontSize * (fontAtlas.ascender() - fontAtlas.descender()) + nodeMarginV * 2.0;

  auto portRadius = static_cast<float>(nodePortWidth * 0.5);
  double portNameHeight = nodePinFontSize * fontAtlas.lineHeight();
  double portTypeHeight = nodePinTypeFontSize * fontAtlas.lineHeight();
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;
  double portStartY = node.position[1] + titleHeight + nodeMarginV;

  std::vector<PortInfo> inputPorts;
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    // Skip unfolded headers (kind 2) — folded headers (kind 1) have aggregate port
    if (i < static_cast<int>(node.inputRowKinds.size())) {
      int kind = node.inputRowKinds[i];
      if (kind == 2) {
        continue;
      }
    }
    PortInfo port;
    port.position = Vec2d(node.position[0], portStartY + i * portLineHeight + portNameHeight * 0.5);
    port.r = static_cast<uint8_t>(portR);
    port.g = static_cast<uint8_t>(portG);
    port.b = static_cast<uint8_t>(portB);
    port.a = static_cast<uint8_t>(portAlpha);
    inputPorts.push_back(port);
  }

  double outputPortStartY = 0.0;
  if (outputPortsTopToBottom) {
    outputPortStartY = portStartY;
  } else {
    outputPortStartY = portStartY + static_cast<double>(node.inputPins.size()) * portLineHeight;
  }

  std::vector<PortInfo> outputPorts;
  double x1 = node.position[0] + node.size[0];
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    // Skip unfolded headers (kind 2) — folded headers (kind 1) have aggregate port
    if (i < static_cast<int>(node.outputRowKinds.size())) {
      int kind = node.outputRowKinds[i];
      if (kind == 2) {
        continue;
      }
    }
    PortInfo port;
    port.position = Vec2d(x1, outputPortStartY + i * portLineHeight + portNameHeight * 0.5);
    port.r = static_cast<uint8_t>(portR);
    port.g = static_cast<uint8_t>(portG);
    port.b = static_cast<uint8_t>(portB);
    port.a = static_cast<uint8_t>(portAlpha);
    outputPorts.push_back(port);
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
      static_cast<float>(titleHeight),
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

bool GraffiNodeRenderer::getOutputPortsTopToBottom() const {
  return false;
}

double GraffiNodeRenderer::getUnselectedStrokeWidth() const {
  double scale = getGlobalScale();
  return 2.0 * scale;
}

double GraffiNodeRenderer::getPortFontSize() const {
  double scale = getGlobalScale();
  return 8.0 * scale;
}

double GraffiNodeRenderer::getPortMarginH() const {
  double scale = getGlobalScale();
  return 16.0 * scale;
}

double GraffiNodeRenderer::getPortMarginV() const {
  double scale = getGlobalScale();
  return 6.0 * scale;
}

double GraffiNodeRenderer::getPortSpacing() const {
  double scale = getGlobalScale();
  return 4.0 * scale;
}

double GraffiNodeRenderer::getTitleFontSize() const {
  double scale = getGlobalScale();
  return 10.0 * scale;
}

double GraffiNodeRenderer::getPortTypeFontSize() const {
  double scale = getGlobalScale();
  return 6.0 * scale;
}

} // namespace noodles
