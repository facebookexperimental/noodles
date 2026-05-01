// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/NodeData.h"
#include "core/RenderConfig.h"

#include <algorithm>
#include <cmath>

namespace noodles {

void GraphModel::rebuildConnectionCache() const {
  connectionCache_.clear();
  for (const auto& [nodeId, _] : nodes) {
    connectionCache_[nodeId] = {};
  }
  for (const auto& link : links) {
    auto srcIt = connectionCache_.find(link.sourceNodeId);
    if (srcIt != connectionCache_.end()) {
      srcIt->second.insert(link.targetNodeId);
    }
    auto tgtIt = connectionCache_.find(link.targetNodeId);
    if (tgtIt != connectionCache_.end()) {
      tgtIt->second.insert(link.sourceNodeId);
    }
  }
  linksChanged_ = false;
}

void GraphModel::buildConnectionCache() {
  rebuildConnectionCache();
}

std::unordered_set<std::string> GraphModel::getConnectedNodeIds(const std::string& nodeId) const {
  if (linksChanged_) {
    rebuildConnectionCache();
  }
  auto it = connectionCache_.find(nodeId);
  if (it != connectionCache_.end()) {
    return it->second;
  }
  return {};
}

void GraphModel::clear() {
  nodes.clear();
  links.clear();
  stickers.clear();
  connectionCache_.clear();
  linksChanged_ = true;
}

void GraphModel::calculateNodeSize(
    NodeData& node,
    const TextWidthCallback& calculateTextWidth,
    const FontMetrics& fontMetrics,
    const RenderConfig* config) {
  RenderConfig defaultConfig;
  if (!config) {
    config = &defaultConfig;
  }

  double globalScale = config->get("globalNodeScale", 2.0);
  double nodeTitleFontSize = config->get("nodeTitleFontSize", 24.0) * globalScale;
  double nodePinFontSize = config->get("nodePinFontSize", 18.0) * globalScale;
  double nodePinTypeFontSize = config->get("nodePinTypeFontSize", 14.0) * globalScale;
  double nodeMarginH = config->get("nodeMarginH", 16.0) * globalScale;
  double nodeMarginV = config->get("nodeMarginV", 18.0) * globalScale;
  double nodePortSpacing = config->get("nodePortSpacing", 1.0) * globalScale;
  double nodePortWidth = config->get("nodePortWidth", 16.0) * globalScale;
  bool outputPortsTopToBottom = config->outputPortsTopToBottom;

  double titleWidth = calculateTextWidth(node.name, nodeTitleFontSize);

  // Account for title bar decorations: [caret] [icon] [title text]
  // These match the layout in textRenderer.py generateNodeTextVertices
  double titleCaretSize = nodeTitleFontSize * 0.5;
  double titleCaretGap = nodeTitleFontSize * 0.15;
  double titleIconSize = nodeTitleFontSize * 1.2;
  double titleDecorWidth = titleCaretSize + titleCaretGap + titleIconSize + titleCaretGap;
  titleWidth += titleDecorWidth;

  double maxInputWidth = 0.0;
  for (const auto& pinName : node.inputPins) {
    double pinWidth = calculateTextWidth(pinName, nodePinFontSize);
    auto it = node.inputPinTypes.find(pinName);
    std::string portType = (it != node.inputPinTypes.end()) ? it->second : node.type;
    if (!portType.empty()) {
      double typeWidth = calculateTextWidth("(" + portType + ")", nodePinTypeFontSize);
      pinWidth = std::max(pinWidth, typeWidth);
    }
    maxInputWidth = std::max(maxInputWidth, pinWidth);
  }

  double maxOutputWidth = 0.0;
  for (const auto& pinName : node.outputPins) {
    double pinWidth = calculateTextWidth(pinName, nodePinFontSize);
    auto it = node.outputPinTypes.find(pinName);
    std::string portType = (it != node.outputPinTypes.end()) ? it->second : node.type;
    if (!portType.empty()) {
      double typeWidth = calculateTextWidth("(" + portType + ")", nodePinTypeFontSize);
      pinWidth = std::max(pinWidth, typeWidth);
    }
    maxOutputWidth = std::max(maxOutputWidth, pinWidth);
  }

  double pinWidth =
      nodePortWidth + maxInputWidth + maxOutputWidth + nodePortWidth + nodeMarginH * 2.0;

  if (config->isGraffiStyle && !node.type.empty()) {
    double nodeTypeFontSize = nodePinTypeFontSize;
    double nodeTypeWidth = calculateTextWidth(node.type, nodeTypeFontSize);
    double gap = 10.0 * globalScale;
    double minWidthForNodeType =
        maxInputWidth + nodeMarginH + gap + nodeTypeWidth + gap + nodeMarginH + maxOutputWidth;
    pinWidth = std::max(pinWidth, minWidthForNodeType);
  }

  double requiredWidth = std::max({titleWidth + nodeMarginH * 2.0, pinWidth, 150.0});

  double titleTextHeight = nodeTitleFontSize * (fontMetrics.ascender - fontMetrics.descender);
  double titleAreaHeight = titleTextHeight + nodeMarginV * 2.0;

  double portNameHeight = nodePinFontSize * fontMetrics.lineHeight;
  double portTypeHeight = nodePinTypeFontSize * fontMetrics.lineHeight;
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;

  int pinCount = 0;
  if (node.titleCollapsed) {
    // Collapsed: no property rows visible
    pinCount = 0;
  } else if (outputPortsTopToBottom) {
    pinCount =
        std::max(static_cast<int>(node.inputPins.size()), static_cast<int>(node.outputPins.size()));
  } else {
    pinCount = static_cast<int>(node.inputPins.size() + node.outputPins.size());
  }

  double pinAreaHeight = (pinCount > 0) ? pinCount * portLineHeight : 0.0;
  double totalHeight = titleAreaHeight + pinAreaHeight + nodeMarginV * 1.0;

  node.size = Vec2d(requiredWidth, totalHeight);
}

} // namespace noodles
