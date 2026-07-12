// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_NODE_DATA_H
#define NOODLES_CORE_NODE_DATA_H

#include "core/Math.h"
#include "core/api.h"
#include "core/pragmas.h"

#include <array>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace noodles {

struct FontMetrics;
struct RenderConfig;

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API NodeData {
  std::string id;
  std::string name;
  std::string type;
  std::string schemaTypeName;
  // Self-versioning (see VersionedVec2d): assignment advances the version, which
  // GraphView._reconcileMovedNodes consumes to refresh position-derived state.
  VersionedVec2d position{};
  uint64_t positionVersion() const {
    return position.version;
  }
  Vec2d size{200.0, 100.0};
  std::vector<std::string> inputPins;
  std::vector<std::string> outputPins;
  std::unordered_map<std::string, std::string> inputPinTypes;
  std::unordered_map<std::string, std::string> outputPinTypes;
  std::vector<int> inputLinks;
  std::vector<int> outputLinks;
  std::vector<int> inputRowKinds; // parallel to inputPins: 0=normal 1=folded 2=unfolded 3=child
  std::vector<int> outputRowKinds; // parallel to outputPins: 0=normal 1=folded 2=unfolded 3=child
  std::vector<int>
      inputRowSlots; // parallel to inputPins: visible row index in authored property order
  std::vector<int>
      outputRowSlots; // parallel to outputPins: visible row index in authored property order
  std::vector<int> displayRowKinds; // global visible rows in authored property order
  // Pins backed by USD relationships (vs plain attributes). The text layout
  // uses these to reserve the relationship-icon right-margin on output rows.
  std::unordered_set<std::string> relationshipInputPins;
  std::unordered_set<std::string> relationshipOutputPins;

  // --- Raw USD-derived CONTENT (the C++ layout producer's inputs) ---
  // Python supplies these from the USD prim; the producer derives the display
  // pins (inputPins/outputPins), row kinds, slots, size, and per-pin centers
  // from them. Keeping the producer's inputs on the struct is what lets one
  // centralized C++ pass own the whole node view-model.
  // Original (pre-grouping) pins + types, in authored property order.
  std::vector<std::string> originalInputPins;
  std::vector<std::string> originalOutputPins;
  std::unordered_map<std::string, std::string> originalInputPinTypes;
  std::unordered_map<std::string, std::string> originalOutputPinTypes;
  // Per-group fold state: group name -> true if collapsed (children hidden).
  std::unordered_map<std::string, bool> foldState;
  // (side, pinName) in USD property composition order; drives slot assignment.
  std::vector<std::pair<std::string, std::string>> orderedPinEntries;
  // Pins originating from the authored inputs:/outputs: direction-group namespaces.
  std::unordered_set<std::string> inputDirectionGroupPins;
  std::unordered_set<std::string> outputDirectionGroupPins;
  // Bare attributes that have ports on BOTH sides (single row, dual circles).
  // dualPinNames: input-side rows that ALSO mirror an output port on the right
  // edge. outputDualPinNames: output-side rows that ALSO mirror an input port on
  // the left edge (symmetric counterpart; see models.py / nodeFactory.py).
  std::unordered_set<std::string> dualPinNames;
  std::unordered_set<std::string> outputDualPinNames;

  bool selected = false;
  bool titleCollapsed = false; // true = all rows hidden, single aggregate port
  std::string uiStyle = "default";
  // USD ui:nodegraph:node:icon path for the title-bar icon (mirrors Python
  // NodeModel._icon_path). Empty means fall back to the default box icon. The
  // C++ icon producer reads this; nothing consumes it yet.
  std::string titleIconPath;

  int textStartIndex = 0;
  int textNumChars = 0;

  int zOrder = 0;

  float displayColorR = -1.0f;
  float displayColorG = -1.0f;
  float displayColorB = -1.0f;

  // Cached layout metrics (set by calculateNodeSize, read by all renderers and
  // interaction code). Single source of truth for node geometry — do NOT
  // recompute these independently.
  double layoutTitleHeight = 0.0;
  double layoutPortStartY = 0.0;
  double layoutPortLineHeight = 0.0;
  double layoutPortNameHeight = 0.0;
  double layoutPortTypeHeight = 0.0;
  int layoutRowCount = 0;
  // Per-pin resolved row CENTER Y, node-LOCAL (add node.position[1] for world).
  // Parallel to inputPins/outputPins. The producer resolves each pin -> visible
  // row slot ONCE here, so every consumer (port circles, text, links, hit-test)
  // reads a pin's vertical center directly and never re-resolves a slot or
  // multiplies an index by a row step — this is what prevents per-row drift.
  std::vector<double> layoutInputCenterY;
  std::vector<double> layoutOutputCenterY;

  // Hidden child pin -> parent group header, produced by the layout pass when a
  // group is folded. Link routing uses these to route a folded child's link
  // through its visible group header. (childPinName -> groupName)
  std::unordered_map<std::string, std::string> foldedInputPinMap;
  std::unordered_map<std::string, std::string> foldedOutputPinMap;
};

struct NOODLES_API LinkData {
  static constexpr double DANGLING_LINK_LENGTH = 50.0;

  std::string sourceNodeId;
  std::string sourcePort;
  std::string targetNodeId;
  std::string targetPort;
  Vec2d start{0.0, 0.0};
  Vec2d end{0.0, 0.0};
  bool selected = false;
  bool hovered = false;
  bool isDangling = false;
  std::string danglingDirection;
  std::array<float, 4> color = {0.0f, 0.0f, 0.0f, 0.0f};
  bool highlighted = false;
  std::array<float, 4> highlightColor = {0.0f, 0.0f, 0.0f, 0.0f};
  bool hasColor = false;
  // Relationship (vs data) link. Bound to Python as `is_relationship_link` to
  // match the existing attribute every consumer already uses; the render path
  // partitions on it (relationship vs regular draw groups).
  bool isRelationship = false;
  // USD property name on the target side. Bound to Python as `targetPropertyName`
  // (the existing attribute name). Empty for whole-prim relationship targets; the
  // render path uses that to detect prim-target links (which inset the end to make
  // room for the arrowhead).
  std::string targetPropertyName;
};

struct NOODLES_API StickerData {
  std::string id;
  std::string label;
  // Plain Vec2d, not VersionedVec2d: stickers are a background render layer with
  // no position-derived state to reconcile (no spatial-index / link hit-testing).
  // Version this like NodeData.position if that ever changes.
  Vec2d position{0.0, 0.0};
  Vec2d size{400.0, 300.0};
  float r = 0.2f, g = 0.2f, b = 0.3f, a = 0.5f;
  bool selected = false;
  std::vector<std::string> nodeIds;
};

struct NOODLES_API FontMetrics {
  double ascender = 0.0;
  double descender = 0.0;
  double lineHeight = 0.0;
};

using TextWidthCallback = std::function<double(const std::string& text, double fontSize)>;

class NOODLES_API GraphModel {
 public:
  std::unordered_map<std::string, NodeData> nodes;
  std::vector<LinkData> links;
  std::vector<StickerData> stickers;

  void buildConnectionCache();
  std::unordered_set<std::string> getConnectedNodeIds(const std::string& nodeId) const;

  // Port-level connection index (finer-grained than getConnectedNodeIds, which
  // is node-granular). True when the (nodeId, port, side) endpoint carries at
  // least one link. The side classification is the canonical link-endpoint side
  // rule (formerly graphView._getLinkEndpointDisplaySide, now C++-owned): a
  // link's source endpoint is the output side and its target endpoint is the
  // input side, except a relationship link whose targetPropertyName forces a
  // side. Rebuilt lazily with the node-adjacency cache, so it is only recomputed
  // on linksChanged_.
  bool isPortConnected(const std::string& nodeId, const std::string& port, bool isOutput) const;
  // True when ANY hidden child of a folded group header carries a link on the
  // given side (mirrors the folded-header "any child connected" aggregate the
  // Python port-highlight pass draws). headerPin is the visible group header.
  bool isFoldedHeaderConnected(
      const std::string& nodeId,
      const std::string& headerPin,
      bool isOutput) const;

  // Synthetic relationship ports for a node: relationship-pin link endpoints
  // (sources/affects) on this node that have NO visible row to attach to, so the
  // port-highlight pass draws a synthetic aggregate port near the title instead
  // (formerly graphView._iterSyntheticRelationshipPorts, now C++-owned). Returns (port, isOutput)
  // pairs. The link-derived candidate set is cached on linksChanged_; the
  // visible-port filter is applied here against the node's CURRENT display pins,
  // so a fold that hides/shows a row updates the result without a links change.
  std::vector<std::pair<std::string, bool>> syntheticRelationshipPortsForNode(
      const std::string& nodeId) const;

  void clear();

  void markLinksChanged() {
    linksChanged_ = true;
  }
  bool isLinksChanged() const {
    return linksChanged_;
  }
  size_t linkCount() const {
    return links.size();
  }

  void calculateNodeSize(
      NodeData& node,
      const TextWidthCallback& calculateTextWidth,
      const FontMetrics& fontMetrics,
      const RenderConfig* config = nullptr);

  // The single, atomic node view-model producer. Derives the display pins +
  // row kinds + folded maps (from the raw original* content), assigns row
  // slots + displayRowKinds, then computes size + per-pin centers — all from
  // one pass so they can never desync. Python only supplies the raw content.
  void layoutNode(
      NodeData& node,
      const TextWidthCallback& calculateTextWidth,
      const FontMetrics& fontMetrics,
      const RenderConfig* config = nullptr);

 private:
  void rebuildConnectionCache() const;
  mutable std::unordered_map<std::string, std::unordered_set<std::string>> connectionCache_;
  // Encoded (nodeId, port, side) keys for every connected port endpoint. Built
  // alongside connectionCache_ in rebuildConnectionCache().
  mutable std::unordered_set<std::string> connectedPorts_;
  // Per-node relationship-pin link endpoints (port, isOutput), the link-derived
  // candidates for synthetic relationship ports. Built alongside the caches; the
  // visible-port filter is deferred to syntheticRelationshipPortsForNode().
  mutable std::unordered_map<std::string, std::set<std::pair<std::string, bool>>>
      relationshipEndpoints_;
  mutable bool linksChanged_ = true;
};

/// Nodes ordered back-to-front by zOrder (pointers into `nodes`). Shared by the
/// text and node-quad render paths so both layers bake the same draw order.
NOODLES_API std::vector<std::pair<const std::string*, NodeData*>> sortNodesByZOrder(
    std::unordered_map<std::string, NodeData>& nodes);

NOODLES_PRAGMA_POP

} // namespace noodles
#endif // NOODLES_CORE_NODE_DATA_H
