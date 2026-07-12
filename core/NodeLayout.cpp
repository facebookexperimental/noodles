// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/NodeLayout.h"

#include "core/NodeData.h"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noodles {

namespace {

bool isFolded(const std::unordered_map<std::string, bool>& foldState, const std::string& group) {
  auto it = foldState.find(group);
  return it != foldState.end() && it->second;
}

// Members of the authored inputs:/outputs: direction group, in originals order
// (ports _get_direction_group_members; returned as a set for membership tests).
std::unordered_set<std::string> directionMemberSet(
    const std::vector<std::string>& originals,
    const std::unordered_set<std::string>& directionGroupPins) {
  std::unordered_set<std::string> out;
  if (directionGroupPins.empty()) {
    return out;
  }
  for (const auto& pin : originals) {
    if (directionGroupPins.count(pin)) {
      out.insert(pin);
    }
  }
  return out;
}

// Port of _build_grouped_display_state (+ _detect_groups / _append_group*),
// for one side. Writes the display pins, row kinds, display pin types, and the
// folded child->header map.
void buildDisplaySide(
    const std::vector<std::string>& originals,
    const std::unordered_map<std::string, std::string>& originalTypes,
    const std::unordered_set<std::string>& directionGroupPins,
    const std::string& directionGroupName,
    const std::unordered_map<std::string, bool>& foldState,
    std::vector<std::string>& displayPins,
    std::vector<int>& rowKinds,
    std::unordered_map<std::string, std::string>& displayPinTypes,
    std::unordered_map<std::string, std::string>& foldedMap) {
  displayPins.clear();
  rowKinds.clear();
  displayPinTypes.clear();
  foldedMap.clear();

  // _detect_groups: prefix -> [full pin names], first-occurrence order.
  std::unordered_map<std::string, std::vector<std::string>> groups;
  for (const auto& pin : originals) {
    auto pos = pin.find(':');
    if (pos != std::string::npos) {
      groups[pin.substr(0, pos)].push_back(pin);
    }
  }

  std::unordered_set<std::string> directionMembers =
      directionMemberSet(originals, directionGroupPins);

  // No namespaced groups and no direction members: pins pass through unchanged
  // and rowKinds stays EMPTY (matches the Python early-return contract that the
  // slot pass relies on — kinds then default to 0).
  if (groups.empty() && directionMembers.empty()) {
    displayPins = originals;
    displayPinTypes = originalTypes;
    return;
  }

  std::unordered_set<std::string> seenPrefixes;
  auto appendHeader = [&](const std::string& groupName, bool folded) {
    displayPins.push_back(groupName);
    rowKinds.push_back(folded ? 1 : 2);
  };
  auto appendChild = [&](const std::string& child, const std::string& groupName, bool folded) {
    if (folded) {
      foldedMap[child] = groupName;
      return;
    }
    displayPins.push_back(child);
    rowKinds.push_back(3);
    auto it = originalTypes.find(child);
    if (it != originalTypes.end()) {
      displayPinTypes[child] = it->second;
    }
  };

  for (const auto& pin : originals) {
    if (directionMembers.count(pin)) {
      bool folded = isFolded(foldState, directionGroupName);
      if (!seenPrefixes.count(directionGroupName)) {
        seenPrefixes.insert(directionGroupName);
        appendHeader(directionGroupName, folded);
      }
      appendChild(pin, directionGroupName, folded);
      continue;
    }
    auto pos = pin.find(':');
    if (pos != std::string::npos) {
      std::string groupName = pin.substr(0, pos);
      if (seenPrefixes.count(groupName)) {
        continue;
      }
      seenPrefixes.insert(groupName);
      bool folded = isFolded(foldState, groupName);
      appendHeader(groupName, folded);
      for (const auto& child : groups[groupName]) {
        appendChild(child, groupName, folded);
      }
      continue;
    }
    // Plain (ungrouped) pin.
    displayPins.push_back(pin);
    rowKinds.push_back(0);
    auto it = originalTypes.find(pin);
    if (it != originalTypes.end()) {
      displayPinTypes[pin] = it->second;
    }
  }
}

// One side's working state during slot assignment (ports the per-side dict in
// _rebuild_row_layout).
struct SideState {
  const std::vector<std::string>* displayPins;
  const std::vector<int>* rowKinds;
  std::vector<int>* rowSlots;
  std::unordered_set<int> assigned;
  std::unordered_set<std::string> seenHeaders;
  std::string directionGroupName;
  std::unordered_set<std::string> directionMembers;
};

// Port of _find_unassigned_display_index; returns -1 for "None".
int findUnassignedDisplayIndex(
    const std::vector<std::string>& displayPins,
    const std::vector<int>& rowKinds,
    const std::string& pinName,
    const std::set<int>& expectedKinds,
    const std::unordered_set<int>& assigned) {
  for (int idx = 0; idx < static_cast<int>(displayPins.size()); ++idx) {
    if (assigned.count(idx) || displayPins[idx] != pinName) {
      continue;
    }
    int kind = idx < static_cast<int>(rowKinds.size()) ? rowKinds[idx] : 0;
    if (expectedKinds.count(kind)) {
      return idx;
    }
  }
  return -1;
}

} // namespace

void buildDisplayPins(NodeData& node) {
  buildDisplaySide(
      node.originalInputPins,
      node.originalInputPinTypes,
      node.inputDirectionGroupPins,
      "inputs",
      node.foldState,
      node.inputPins,
      node.inputRowKinds,
      node.inputPinTypes,
      node.foldedInputPinMap);
  buildDisplaySide(
      node.originalOutputPins,
      node.originalOutputPinTypes,
      node.outputDirectionGroupPins,
      "outputs",
      node.foldState,
      node.outputPins,
      node.outputRowKinds,
      node.outputPinTypes,
      node.foldedOutputPinMap);
}

void assignRowSlots(NodeData& node) {
  const std::vector<std::string>& inputPins = node.inputPins;
  const std::vector<std::string>& outputPins = node.outputPins;
  const std::vector<int>& inputRowKinds = node.inputRowKinds;
  const std::vector<int>& outputRowKinds = node.outputRowKinds;

  node.inputRowSlots.assign(inputPins.size(), -1);
  node.outputRowSlots.assign(outputPins.size(), -1);
  node.displayRowKinds.clear();

  if (inputPins.empty() && outputPins.empty()) {
    return;
  }

  // No authored property order: identity slots (port the no-ordered-entries
  // branch). Each pin lands on its own row; kinds map straight through.
  if (node.orderedPinEntries.empty()) {
    for (int i = 0; i < static_cast<int>(inputPins.size()); ++i) {
      node.inputRowSlots[i] = i;
    }
    for (int i = 0; i < static_cast<int>(outputPins.size()); ++i) {
      node.outputRowSlots[i] = i;
    }
    int maxSlot = -1;
    for (int s : node.inputRowSlots) {
      maxSlot = std::max(maxSlot, s);
    }
    for (int s : node.outputRowSlots) {
      maxSlot = std::max(maxSlot, s);
    }
    int rowCount = maxSlot >= 0 ? maxSlot + 1 : 0;
    node.displayRowKinds.assign(rowCount, 0);
    for (int idx = 0; idx < static_cast<int>(inputRowKinds.size()); ++idx) {
      int slot = idx < static_cast<int>(node.inputRowSlots.size()) ? node.inputRowSlots[idx] : idx;
      if (slot >= 0 && slot < rowCount && inputRowKinds[idx] != 0) {
        node.displayRowKinds[slot] = inputRowKinds[idx];
      }
    }
    for (int idx = 0; idx < static_cast<int>(outputRowKinds.size()); ++idx) {
      int slot =
          idx < static_cast<int>(node.outputRowSlots.size()) ? node.outputRowSlots[idx] : idx;
      if (slot >= 0 && slot < rowCount && node.displayRowKinds[slot] == 0 &&
          outputRowKinds[idx] != 0) {
        node.displayRowKinds[slot] = outputRowKinds[idx];
      }
    }
    return;
  }

  SideState inputState{
      &inputPins,
      &inputRowKinds,
      &node.inputRowSlots,
      {},
      {},
      "inputs",
      directionMemberSet(node.originalInputPins, node.inputDirectionGroupPins)};
  SideState outputState{
      &outputPins,
      &outputRowKinds,
      &node.outputRowSlots,
      {},
      {},
      "outputs",
      directionMemberSet(node.originalOutputPins, node.outputDirectionGroupPins)};

  auto pick = [&](const std::string& sideKey) -> SideState* {
    if (sideKey == "input") {
      return &inputState;
    }
    if (sideKey == "output") {
      return &outputState;
    }
    return nullptr;
  };

  auto assignSlot = [&](SideState& st, int displayIndex) {
    if (displayIndex < 0 || st.assigned.count(displayIndex)) {
      return;
    }
    st.assigned.insert(displayIndex);
    (*st.rowSlots)[displayIndex] = static_cast<int>(node.displayRowKinds.size());
    int kind =
        displayIndex < static_cast<int>(st.rowKinds->size()) ? (*st.rowKinds)[displayIndex] : 0;
    node.displayRowKinds.push_back(kind);
  };

  // A namespaced prefix (e.g. "skel") can head a group on BOTH sides at once —
  // its dual attributes make an input header while its relationships make an
  // output header. Those are the same logical group, so both headers share ONE
  // row slot: the first side to reach the group creates the row, the second
  // reuses it (no new displayRowKind). The renderer draws the caret/label once
  // and folds them together (shared foldState key), while each side keeps its
  // own header row entry so a folded group still shows a connected aggregate
  // port on whichever edge(s) carry links. Direction groups ("inputs"/
  // "outputs") are inherently one-sided, so they are never shared.
  std::unordered_map<std::string, int> sharedHeaderSlots;
  auto assignHeaderSlot =
      [&](SideState& st, int headerIdx, const std::string& groupName, bool shareAcrossSides) {
        if (headerIdx < 0 || st.assigned.count(headerIdx)) {
          return;
        }
        if (shareAcrossSides) {
          auto it = sharedHeaderSlots.find(groupName);
          if (it != sharedHeaderSlots.end()) {
            // Reuse the row the other side already created for this group.
            st.assigned.insert(headerIdx);
            (*st.rowSlots)[headerIdx] = it->second;
            return;
          }
        }
        int slot = static_cast<int>(node.displayRowKinds.size());
        assignSlot(st, headerIdx);
        if (shareAcrossSides) {
          sharedHeaderSlots[groupName] = slot;
        }
      };

  static const std::set<int> kHeaderKinds{1, 2};
  static const std::set<int> kChildKinds{3};
  static const std::set<int> kNormalKinds{0};

  auto assignGroupedEntry = [&](SideState& st,
                                const std::string& groupName,
                                const std::string& pinName,
                                bool shareHeaderAcrossSides) {
    if (!st.seenHeaders.count(groupName)) {
      int headerIdx = findUnassignedDisplayIndex(
          *st.displayPins, *st.rowKinds, groupName, kHeaderKinds, st.assigned);
      assignHeaderSlot(st, headerIdx, groupName, shareHeaderAcrossSides);
      st.seenHeaders.insert(groupName);
    }
    if (isFolded(node.foldState, groupName)) {
      return;
    }
    int childIdx = findUnassignedDisplayIndex(
        *st.displayPins, *st.rowKinds, pinName, kChildKinds, st.assigned);
    assignSlot(st, childIdx);
  };

  for (const auto& entry : node.orderedPinEntries) {
    SideState* stp = pick(entry.first);
    if (!stp) {
      continue;
    }
    SideState& st = *stp;
    if (st.displayPins->empty()) {
      continue;
    }
    const std::string& pinName = entry.second;

    if (st.directionMembers.count(pinName)) {
      // Direction groups are one-sided; never shared across edges.
      assignGroupedEntry(st, st.directionGroupName, pinName, /*shareHeaderAcrossSides=*/false);
      continue;
    }
    auto pos = pinName.find(':');
    if (pos != std::string::npos) {
      assignGroupedEntry(st, pinName.substr(0, pos), pinName, /*shareHeaderAcrossSides=*/true);
      continue;
    }
    int rowIdx = findUnassignedDisplayIndex(
        *st.displayPins, *st.rowKinds, pinName, kNormalKinds, st.assigned);
    assignSlot(st, rowIdx);
  }

  // Leftover sweep for any display pins not covered by orderedPinEntries
  // (input side then output side, matching the Python dict-values order).
  // Namespaced group headers still share a row across sides; direction-group
  // headers ("inputs"/"outputs") do not collide since each is one-sided.
  for (SideState* stp : {&inputState, &outputState}) {
    SideState& st = *stp;
    for (int displayIndex = 0; displayIndex < static_cast<int>(st.displayPins->size());
         ++displayIndex) {
      if (st.assigned.count(displayIndex)) {
        continue;
      }
      int kind =
          displayIndex < static_cast<int>(st.rowKinds->size()) ? (*st.rowKinds)[displayIndex] : 0;
      if (kind == 1 || kind == 2) {
        const std::string& headerName = (*st.displayPins)[displayIndex];
        bool shareAcrossSides = headerName != st.directionGroupName;
        assignHeaderSlot(st, displayIndex, headerName, shareAcrossSides);
      } else {
        assignSlot(st, displayIndex);
      }
    }
  }
}

void GraphModel::layoutNode(
    NodeData& node,
    const TextWidthCallback& calculateTextWidth,
    const FontMetrics& fontMetrics,
    const RenderConfig* config) {
  buildDisplayPins(node);
  assignRowSlots(node);
  calculateNodeSize(node, calculateTextWidth, fontMetrics, config);
}

} // namespace noodles
