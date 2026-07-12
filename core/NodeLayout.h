// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_NODE_LAYOUT_H
#define NOODLES_CORE_NODE_LAYOUT_H

#include "core/api.h"
#include "core/pragmas.h"

namespace noodles {

struct NodeData;

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

// Topological node layout — pure C++ ports of the former Python producers.
// These are text-metric AGNOSTIC: they map raw USD-derived content (the
// original* fields + fold state + ordered entries + direction groups) into the
// display structure and row slots that the size/center pass then consumes.

// Derive the display pins from the raw originals: insert group headers for
// `:`-namespaced and direction-group pins, hide folded children (recording them
// in the folded*PinMap for link routing), and classify each display row
// (inputRowKinds/outputRowKinds: 0=normal 1=folded-header 2=unfolded-header
// 3=child). Writes node.inputPins/outputPins, inputPinTypes/outputPinTypes,
// inputRowKinds/outputRowKinds, foldedInputPinMap/foldedOutputPinMap.
NOODLES_API void buildDisplayPins(NodeData& node);

// Assign each display pin to a visible row slot (inputRowSlots/outputRowSlots)
// and build displayRowKinds, walking orderedPinEntries in USD composition order
// (with an identity fallback when no ordering is authored). Must run after
// buildDisplayPins.
NOODLES_API void assignRowSlots(NodeData& node);

NOODLES_PRAGMA_POP

} // namespace noodles
#endif // NOODLES_CORE_NODE_LAYOUT_H
