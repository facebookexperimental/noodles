// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/NodeData.h"
#include "core/RenderConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <string_view>

namespace noodles {

namespace {

// Direction-hint namespaces, mirrored from pinUtils.py. The trailing ':' makes
// each an exact namespace-token match ("internal:foo" does not match "in:").
constexpr std::array<std::string_view, 3> kInputHintPrefixes{"inputs:", "input:", "in:"};
constexpr std::array<std::string_view, 3> kOutputHintPrefixes{"outputs:", "output:", "out:"};

bool startsWith(const std::string& s, std::string_view prefix) {
  return s.size() >= prefix.size() && std::string_view(s).substr(0, prefix.size()) == prefix;
}

// Side a direction-hint namespace forces, mirroring pinUtils.split_direction_hint
// (we only need the side, not the stripped remainder).
enum class HintSide { kNone, kInput, kOutput };
HintSide directionHint(const std::string& name) {
  for (const auto prefix : kInputHintPrefixes) {
    if (startsWith(name, prefix)) {
      return HintSide::kInput;
    }
  }
  for (const auto prefix : kOutputHintPrefixes) {
    if (startsWith(name, prefix)) {
      return HintSide::kOutput;
    }
  }
  return HintSide::kNone;
}

// Relationship pin name sets, mirrored from pinUtils.py.
bool isRelationshipInputPin(const std::string& name) {
  return name == "sources";
}
bool isRelationshipPin(const std::string& name) {
  return name == "sources" || name == "affects";
}

// The inherent output-side a property forces, mirroring
// graphView._getPropertyOutputSide: true=output, false=input, nullopt=no
// inherent side (bare/other-namespaced attrs defer to the link's own direction).
std::optional<bool> propertyOutputSide(const std::string& name) {
  HintSide side = directionHint(name);
  if (side == HintSide::kOutput || isRelationshipPin(name)) {
    return true;
  }
  if (side == HintSide::kInput || isRelationshipInputPin(name)) {
    return false;
  }
  return std::nullopt;
}

// Encode a (nodeId, port, side) endpoint into a single set key. The unit
// separator (0x1f) never appears in USD paths or property names, so the key is
// unambiguous without a custom hash.
std::string portKey(const std::string& nodeId, const std::string& port, bool isOutput) {
  std::string key;
  key.reserve(nodeId.size() + port.size() + 3);
  key.append(nodeId);
  key.push_back('\x1f');
  key.append(port);
  key.push_back('\x1f');
  key.push_back(isOutput ? '1' : '0');
  return key;
}

// Whether a RELATIONSHIP pin has a visible row on the given side. has_visible_port
// (models.py) is "pin in displayPins OR pin in the dual set", but relationship
// pins are explicitly excluded from both dual sets (nodeFactory.py / models.py),
// so for a relationship pin it reduces to membership in the display pins. (Only
// relationship pins reach this; non-relationship pins are never synthetic.)
bool relationshipPinHasVisiblePort(const NodeData& node, const std::string& pin, bool isOutput) {
  const std::vector<std::string>& pins = isOutput ? node.outputPins : node.inputPins;
  return std::find(pins.begin(), pins.end(), pin) != pins.end();
}

int getVisibleRowCount(const NodeData& node) {
  if (node.titleCollapsed) {
    return 0;
  }
  if (!node.displayRowKinds.empty()) {
    return static_cast<int>(node.displayRowKinds.size());
  }

  int maxSlot = -1;
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    int slot = (i < static_cast<int>(node.inputRowSlots.size())) ? node.inputRowSlots[i] : i;
    maxSlot = std::max(maxSlot, slot);
  }
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    int slot = (i < static_cast<int>(node.outputRowSlots.size())) ? node.outputRowSlots[i] : i;
    maxSlot = std::max(maxSlot, slot);
  }
  if (maxSlot >= 0) {
    return maxSlot + 1;
  }

  return std::max(
      static_cast<int>(node.inputPins.size()), static_cast<int>(node.outputPins.size()));
}

} // namespace

void GraphModel::rebuildConnectionCache() const {
  connectionCache_.clear();
  connectedPorts_.clear();
  relationshipEndpoints_.clear();
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

    // Port-level index. LinkData carries no sourcePropertyName, so the source
    // endpoint never has an inherent side and is always the output side. The
    // target endpoint defaults to the input side, overridden only when a
    // relationship link's targetPropertyName forces a side. This is the canonical
    // link-endpoint side rule (formerly graphView._getLinkEndpointDisplaySide,
    // now C++-owned), including its "no pin and no property" empty-endpoint skip.
    if (!link.sourcePort.empty()) {
      connectedPorts_.insert(portKey(link.sourceNodeId, link.sourcePort, /*isOutput=*/true));
      if (isRelationshipPin(link.sourcePort)) {
        relationshipEndpoints_[link.sourceNodeId].emplace(link.sourcePort, /*isOutput=*/true);
      }
    }
    if (!link.targetPort.empty() || !link.targetPropertyName.empty()) {
      bool targetSide = false; // default: input side
      if (link.isRelationship) {
        std::optional<bool> forced = propertyOutputSide(link.targetPropertyName);
        if (forced.has_value()) {
          targetSide = *forced;
        }
      }
      connectedPorts_.insert(portKey(link.targetNodeId, link.targetPort, targetSide));
      if (isRelationshipPin(link.targetPort)) {
        relationshipEndpoints_[link.targetNodeId].emplace(link.targetPort, targetSide);
      }
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

bool GraphModel::isPortConnected(const std::string& nodeId, const std::string& port, bool isOutput)
    const {
  if (linksChanged_) {
    rebuildConnectionCache();
  }
  return connectedPorts_.count(portKey(nodeId, port, isOutput)) > 0;
}

bool GraphModel::isFoldedHeaderConnected(
    const std::string& nodeId,
    const std::string& headerPin,
    bool isOutput) const {
  if (linksChanged_) {
    rebuildConnectionCache();
  }
  auto nodeIt = nodes.find(nodeId);
  if (nodeIt == nodes.end()) {
    return false;
  }
  const auto& foldedMap =
      isOutput ? nodeIt->second.foldedOutputPinMap : nodeIt->second.foldedInputPinMap;
  for (const auto& [childPin, parentHeader] : foldedMap) {
    if (parentHeader == headerPin &&
        connectedPorts_.count(portKey(nodeId, childPin, isOutput)) > 0) {
      return true;
    }
  }
  return false;
}

std::vector<std::pair<std::string, bool>> GraphModel::syntheticRelationshipPortsForNode(
    const std::string& nodeId) const {
  if (linksChanged_) {
    rebuildConnectionCache();
  }
  std::vector<std::pair<std::string, bool>> result;
  auto candidatesIt = relationshipEndpoints_.find(nodeId);
  if (candidatesIt == relationshipEndpoints_.end()) {
    return result;
  }
  auto nodeIt = nodes.find(nodeId);
  if (nodeIt == nodes.end()) {
    return result;
  }
  for (const auto& [port, isOutput] : candidatesIt->second) {
    if (!relationshipPinHasVisiblePort(nodeIt->second, port, isOutput)) {
      result.emplace_back(port, isOutput);
    }
  }
  return result;
}

void GraphModel::clear() {
  nodes.clear();
  links.clear();
  stickers.clear();
  connectionCache_.clear();
  connectedPorts_.clear();
  relationshipEndpoints_.clear();
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

  double nodeTitleFontSize = config->get("nodeTitleFontSize", 48.0);
  double nodePinFontSize = config->get("nodePinFontSize", 36.0);
  double nodePinTypeFontSize = config->get("nodePinTypeFontSize", 28.0);
  double nodeMarginH = config->get("nodeMarginH", 32.0);
  double nodeMarginV = config->get("nodeMarginV", 36.0);
  double nodePortSpacing = config->get("nodePortSpacing", 2.0);
  double nodePortWidth = config->get("nodePortWidth", 32.0);
  double titleWidth = calculateTextWidth(node.name, nodeTitleFontSize);
  double portNameHeight = nodePinFontSize * fontMetrics.lineHeight;
  // When pin-type labels are hidden, rows carry no type subtitle, so the row
  // height (and every layout metric derived from portLineHeight) shrinks.
  double portTypeHeight =
      config->showPinTypeLabels ? nodePinTypeFontSize * fontMetrics.lineHeight : 0.0;
  double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;
  double rowIconSize = portLineHeight * 0.64;
  double relationshipRowIconSize = portLineHeight * 0.72;
  double rowIconGap = nodePinFontSize * 0.25;
  double inputRowDecorWidth = rowIconSize + rowIconGap;
  // Relationship metadata is resolved in Python, so reserve the right-side
  // relationship marker width conservatively for output labels.
  double outputRowDecorWidth = relationshipRowIconSize + rowIconGap;

  // Account for title bar decorations: [caret] [icon] [title text]
  // These match the layout in textRenderer.py generateNodeTextVertices
  double titleCaretSize = nodeTitleFontSize * 0.5;
  double titleCaretGap = nodeTitleFontSize * 0.15;
  double titleIconSize = nodeTitleFontSize * 1.2;
  double titleDecorWidth = titleCaretSize + titleCaretGap + titleIconSize + titleCaretGap;
  titleWidth += titleDecorWidth;

  // Schema type subtitle width
  if (!node.schemaTypeName.empty()) {
    double schemaTypeWidth =
        calculateTextWidth(
            node.schemaTypeName, nodeTitleFontSize * RenderConfig::kSchemaTypeFontRatio) +
        titleDecorWidth;
    titleWidth = std::max(titleWidth, schemaTypeWidth);
  }

  double maxInputWidth = 0.0;
  for (const auto& pinName : node.inputPins) {
    double pinWidth = calculateTextWidth(pinName, nodePinFontSize);
    auto it = node.inputPinTypes.find(pinName);
    std::string portType = (it != node.inputPinTypes.end()) ? it->second : node.type;
    if (!portType.empty() && config->showPinTypeLabels) {
      double typeWidth = calculateTextWidth("(" + portType + ")", nodePinTypeFontSize);
      pinWidth = std::max(pinWidth, typeWidth);
    }
    maxInputWidth = std::max(maxInputWidth, pinWidth);
  }
  if (!node.inputPins.empty() || !node.outputPins.empty()) {
    maxInputWidth += inputRowDecorWidth;
  }

  double maxOutputWidth = 0.0;
  for (const auto& pinName : node.outputPins) {
    double pinWidth = calculateTextWidth(pinName, nodePinFontSize);
    auto it = node.outputPinTypes.find(pinName);
    std::string portType = (it != node.outputPinTypes.end()) ? it->second : node.type;
    if (!portType.empty() && config->showPinTypeLabels) {
      double typeWidth = calculateTextWidth("(" + portType + ")", nodePinTypeFontSize);
      pinWidth = std::max(pinWidth, typeWidth);
    }
    maxOutputWidth = std::max(maxOutputWidth, pinWidth);
  }
  if (!node.inputPins.empty() || !node.outputPins.empty()) {
    maxOutputWidth += outputRowDecorWidth;
  }

  double pinWidth =
      nodePortWidth + maxInputWidth + maxOutputWidth + nodePortWidth + nodeMarginH * 2.0;

  if (config->isGraffiStyle && !node.type.empty()) {
    double nodeTypeFontSize = nodePinTypeFontSize;
    double nodeTypeWidth = calculateTextWidth(node.type, nodeTypeFontSize);
    double gap = 20.0;
    double minWidthForNodeType =
        maxInputWidth + nodeMarginH + gap + nodeTypeWidth + gap + nodeMarginH + maxOutputWidth;
    pinWidth = std::max(pinWidth, minWidthForNodeType);
  }

  double requiredWidth = std::max({titleWidth + nodeMarginH * 2.0, pinWidth, 150.0});

  double titleTextHeight = nodeTitleFontSize * (fontMetrics.ascender - fontMetrics.descender);
  double titleAreaHeight = titleTextHeight + nodeMarginV * 2.0;

  // Add height for schema type subtitle
  if (!node.schemaTypeName.empty()) {
    double schemaTypeHeight =
        nodeTitleFontSize * RenderConfig::kSchemaTypeFontRatio * fontMetrics.lineHeight;
    titleAreaHeight += schemaTypeHeight;
  }

  int pinCount = getVisibleRowCount(node);

  double pinAreaHeight = (pinCount > 0) ? pinCount * portLineHeight : 0.0;
  // No trailing bottom margin: the node ends exactly at the last row's bottom
  // edge (or, when collapsed, at the title area's bottom). The title's own
  // vertical padding lives inside the colored title bar, so an extra
  // node-background margin here would read as an odd gap below the last row /
  // below the title. The last row's bottom stripe is rounded to the node's
  // bottom corners by the fragment SDF.
  double totalHeight = titleAreaHeight + pinAreaHeight;

  node.size = Vec2d(requiredWidth, totalHeight);

  // Cache layout metrics so every renderer + interaction path reads from one
  // source instead of recomputing independently.
  node.layoutTitleHeight = titleAreaHeight;
  node.layoutPortStartY = titleAreaHeight;
  node.layoutPortLineHeight = portLineHeight;
  node.layoutPortNameHeight = portNameHeight;
  node.layoutPortTypeHeight = portTypeHeight;
  node.layoutRowCount = pinCount;

  // Resolve each pin to its visible row ONCE and cache the band-center Y
  // (node-local). This is the authoritative per-pin vertical position; all
  // renderers/interaction read it instead of re-resolving slots or multiplying
  // a row index by portLineHeight (which is what caused per-row drift).
  auto resolveSlot = [](const std::vector<int>& slots, int i) {
    return (i < static_cast<int>(slots.size())) ? slots[i] : i;
  };
  auto centerForPin = [&](const std::vector<int>& slots, int i) -> double {
    if (node.titleCollapsed) {
      return node.size[1] * 0.5; // collapsed: all ports converge to node center
    }
    int slot = resolveSlot(slots, i);
    return titleAreaHeight + slot * portLineHeight + portLineHeight * 0.5;
  };
  node.layoutInputCenterY.clear();
  node.layoutInputCenterY.reserve(node.inputPins.size());
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    node.layoutInputCenterY.push_back(centerForPin(node.inputRowSlots, i));
  }
  node.layoutOutputCenterY.clear();
  node.layoutOutputCenterY.reserve(node.outputPins.size());
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    node.layoutOutputCenterY.push_back(centerForPin(node.outputRowSlots, i));
  }
}

std::vector<std::pair<const std::string*, NodeData*>> sortNodesByZOrder(
    std::unordered_map<std::string, NodeData>& nodes) {
  std::vector<std::pair<const std::string*, NodeData*>> sorted;
  sorted.reserve(nodes.size());
  for (auto& [nodeId, node] : nodes) {
    sorted.emplace_back(&nodeId, &node);
  }
  std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    return a.second->zOrder < b.second->zOrder;
  });
  return sorted;
}

} // namespace noodles
