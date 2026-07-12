// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_GRAPH_LAYOUT_H
#define NOODLES_CORE_GRAPH_LAYOUT_H

#include "core/api.h"
#include "core/pragmas.h"

namespace noodles {

class GraphModel;

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

// Tunable parameters for layoutGraph(). The gap defaults are sized for the
// default node render scale (title/pin fonts ~48/36px); the gaps are in the same
// units as node sizes, so if you change the node font sizes, scale these gaps
// proportionally to keep the same visual density.
//
// NOTE: this is graph-level layout (placing nodes), distinct from the per-node
// NodeLayout producer (display pins / row slots / sizing) in NodeLayout.h.
struct NOODLES_API LayoutParams {
  double columnGap = 800.0; // horizontal gap between layers (columns)
  double intraClusterGap = 80.0; // vertical gap between nodes within a cluster
  double interClusterGap = 300.0; // vertical gap between clusters in a column
  double componentGap = 400.0; // vertical gap between connected components
  double tightClusterMinSpacing = 400.0; // min cluster spacing in the Y-seeding pass
  double tightClusterGap = 40.0; // per-node gap in the Y-seeding pass
  double inputWeight = 3.0; // weight of incoming edges in vertical placement
  double outputWeight = 1.0; // weight of outgoing edges in vertical placement
  double centerBias = 0.5; // blend pin-alignment toward center-alignment (0=pin, 1=center)
  double linearCenterBias = 1.0; // center bias for linear-chain nodes
  double jitterFractionX = 0.3; // deterministic X jitter as a fraction of columnGap (0 disables)
  double jitterFractionY =
      0.1; // deterministic Y jitter as a fraction of interClusterGap (0 disables)
};

// Auto-layout the graph in place using a Sugiyama-style layered algorithm. Only
// DATA-FLOW (port) links drive the
// layout; RELATIONSHIP links (LinkData::isRelationship) are excluded from every
// structural phase, so their top-center anchors can never distort node ranks --
// they are drawn over the placed nodes by the existing link rebuild. Mutates only
// NodeData::position; node sizes and per-pin centers must already be populated
// (GraphModel::layoutNode / calculateNodeSize).
NOODLES_API void layoutGraph(GraphModel& graph, const LayoutParams& params = {});

NOODLES_PRAGMA_POP

} // namespace noodles
#endif // NOODLES_CORE_GRAPH_LAYOUT_H
