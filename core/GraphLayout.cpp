// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/GraphLayout.h"

#include "core/Math.h"
#include "core/NodeData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Graph auto-layout: a self-contained Sugiyama-style layered algorithm. Key
// design properties:
//   * It is a pure producer over GraphModel/NodeData -- no UI-framework or
//     widget state is touched; only the model is mutated.
//   * RELATIONSHIP links are excluded from every structural phase (the single
//     "relationship seam" is buildDataAdjacency); only data-flow (port) edges
//     drive ranking, ordering and coordinates. Relationship links -- including
//     the whole-prim ones anchored at a node's top-center -- are drawn over the
//     placed nodes by the existing link rebuild, so their anchor can never
//     distort node ranks.
//   * Node iteration is sorted by id wherever order affects the result, and
//     jitter uses a stable hash, so the same graph always lays out the same way
//     (avoiding any dependence on unordered_map iteration order or std::hash).
namespace noodles {
namespace {

// A connection from one node to another, expressed relative to the OWNING node:
// `other` is the connected node, `ownPin` is the owning node's pin, `otherPin`
// is the connected node's pin.
struct NeighborRef {
  std::string other;
  std::string ownPin;
  std::string otherPin;
};

// Data-flow adjacency (relationship links excluded). `incoming[n]` are upstream
// producers feeding n; `outgoing[n]` are downstream consumers fed by n.
struct Adjacency {
  std::unordered_map<std::string, std::vector<NeighborRef>> incoming;
  std::unordered_map<std::string, std::vector<NeighborRef>> outgoing;
};

const std::vector<NeighborRef>& neighbors(
    const std::unordered_map<std::string, std::vector<NeighborRef>>& m,
    const std::string& id) {
  static const std::vector<NeighborRef> kEmpty;
  const auto it = m.find(id);
  return (it == m.end()) ? kEmpty : it->second;
}

// THE relationship seam: build adjacency from data-flow links only. Any link with
// isRelationship set is skipped entirely, so it contributes to no rank, order or
// coordinate.
Adjacency buildDataAdjacency(const GraphModel& graph) {
  Adjacency adj;
  for (const LinkData& link : graph.links) {
    if (link.isRelationship) {
      continue;
    }
    if (link.sourceNodeId.empty() || link.targetNodeId.empty()) {
      continue;
    }
    if (graph.nodes.find(link.sourceNodeId) == graph.nodes.end() ||
        graph.nodes.find(link.targetNodeId) == graph.nodes.end()) {
      continue;
    }
    adj.outgoing[link.sourceNodeId].push_back(
        NeighborRef{link.targetNodeId, link.sourcePort, link.targetPort});
    adj.incoming[link.targetNodeId].push_back(
        NeighborRef{link.sourceNodeId, link.targetPort, link.sourcePort});
  }
  return adj;
}

// All node ids, sorted, for deterministic iteration.
std::vector<std::string> sortedNodeIds(const GraphModel& graph) {
  std::vector<std::string> ids;
  ids.reserve(graph.nodes.size());
  for (const auto& [id, node] : graph.nodes) {
    ids.push_back(id);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

// Node-local Y center of a pin, from the cached per-pin centers populated by
// calculateNodeSize. Falls back to the node's vertical center if the pin has no
// visible row (e.g. a folded child).
double pinCenterYLocal(const NodeData& node, const std::string& pin, bool isInput) {
  const std::vector<std::string>& pins = isInput ? node.inputPins : node.outputPins;
  const std::vector<double>& centers = isInput ? node.layoutInputCenterY : node.layoutOutputCenterY;
  for (std::size_t i = 0; i < pins.size(); ++i) {
    if (pins[i] == pin) {
      return (i < centers.size()) ? centers[i] : node.size[1] * 0.5;
    }
  }
  return node.size[1] * 0.5;
}

// Fractional position of a pin among its side's pins (0 if <=1 pin).
double pinFraction(const NodeData& node, const std::string& pin, bool isInput) {
  const std::vector<std::string>& pins = isInput ? node.inputPins : node.outputPins;
  const int total = static_cast<int>(pins.size());
  if (total <= 1) {
    return 0.0;
  }
  for (int i = 0; i < total; ++i) {
    if (pins[i] == pin) {
      return static_cast<double>(i) / total;
    }
  }
  return 0.0;
}

// FNV-1a 64-bit; deterministic across platforms (unlike std::hash) so jitter is
// stable everywhere.
uint64_t stableHash(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (const unsigned char c : s) {
    h ^= static_cast<uint64_t>(c);
    h *= 1099511628211ull;
  }
  return h;
}

// Working state shared by the layout phases.
struct Ctx {
  std::unordered_map<std::string, NodeData>& nodes;
  const Adjacency& adj;
  const LayoutParams& p;
};

NodeData& nodeOf(Ctx& ctx, const std::string& id) {
  return ctx.nodes.at(id);
}

struct NodeCluster {
  std::vector<std::string> nodes;
  double centroidY = 0.0;
  double totalHeight = 0.0;
};

// Split the graph into connected components over DATA edges. Iterates a sorted id
// list and sorts each component, so the result is deterministic.
std::vector<std::vector<std::string>> findConnectedComponents(
    const std::vector<std::string>& sortedIds,
    const Adjacency& adj) {
  std::vector<std::vector<std::string>> components;
  std::unordered_set<std::string> visited;
  for (const std::string& start : sortedIds) {
    if (visited.count(start)) {
      continue;
    }
    std::vector<std::string> component;
    std::queue<std::string> bfs;
    bfs.push(start);
    visited.insert(start);
    while (!bfs.empty()) {
      const std::string current = bfs.front();
      bfs.pop();
      component.push_back(current);
      for (const NeighborRef& n : neighbors(adj.incoming, current)) {
        if (!visited.count(n.other)) {
          visited.insert(n.other);
          bfs.push(n.other);
        }
      }
      for (const NeighborRef& n : neighbors(adj.outgoing, current)) {
        if (!visited.count(n.other)) {
          visited.insert(n.other);
          bfs.push(n.other);
        }
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  return components;
}

// Longest-path layer (rank) assignment via Kahn's algorithm. Cyclic nodes that
// never reach in-degree 0 are placed at layer 0 so they still get a position.
std::unordered_map<std::string, int> assignLayers(
    const std::vector<std::string>& component,
    const Adjacency& adj) {
  const std::unordered_set<std::string> inComponent(component.begin(), component.end());
  std::unordered_map<std::string, int> layers;
  std::unordered_map<std::string, int> inDegree;
  for (const std::string& id : component) {
    inDegree[id] = 0;
  }
  for (const std::string& id : component) {
    for (const NeighborRef& n : neighbors(adj.outgoing, id)) {
      if (inComponent.count(n.other)) {
        inDegree[n.other]++;
      }
    }
  }

  std::vector<std::string> seeds;
  for (const std::string& id : component) {
    if (inDegree[id] == 0) {
      seeds.push_back(id);
    }
  }
  std::sort(seeds.begin(), seeds.end());

  std::queue<std::string> queue;
  for (const std::string& id : seeds) {
    layers[id] = 0;
    queue.push(id);
  }
  while (!queue.empty()) {
    const std::string current = queue.front();
    queue.pop();
    std::vector<std::string> downstream;
    for (const NeighborRef& n : neighbors(adj.outgoing, current)) {
      if (inComponent.count(n.other)) {
        downstream.push_back(n.other);
      }
    }
    std::sort(downstream.begin(), downstream.end());
    for (const std::string& nb : downstream) {
      const auto it = layers.find(nb);
      const int existing = (it == layers.end()) ? 0 : it->second;
      layers[nb] = std::max(existing, layers[current] + 1);
      if (--inDegree[nb] == 0) {
        queue.push(nb);
      }
    }
  }

  for (const std::string& id : component) {
    if (!layers.count(id)) {
      layers[id] = 0;
    }
  }
  return layers;
}

bool hasDownstreamInNextLevel(
    const std::string& id,
    int level,
    const std::unordered_map<std::string, int>& layers,
    const Adjacency& adj) {
  for (const NeighborRef& n : neighbors(adj.outgoing, id)) {
    const auto it = layers.find(n.other);
    if (it != layers.end() && it->second == level + 1) {
      return true;
    }
  }
  return false;
}

// Pull nodes leftward to remove slack between a node and its dependencies /
// consumers. Iterates `component` (sorted) for determinism.
void compactLayers(
    const std::vector<std::string>& component,
    std::unordered_map<std::string, int>& layers,
    const Adjacency& adj) {
  constexpr int kMaxIterations = 20;
  bool changed = true;
  int iteration = 0;
  while (changed && iteration < kMaxIterations) {
    changed = false;
    iteration++;
    for (const std::string& nodeId : component) {
      const int level = layers[nodeId];
      if (level > 0) {
        // Strategy 1: basic promotion (one level at a time).
        if (!hasDownstreamInNextLevel(nodeId, level, layers, adj)) {
          bool allDepsReady = true;
          for (const NeighborRef& in : neighbors(adj.incoming, nodeId)) {
            const auto it = layers.find(in.other);
            if (it != layers.end() && it->second >= level - 1) {
              allDepsReady = false;
              break;
            }
          }
          if (allDepsReady) {
            layers[nodeId] = level - 1;
            changed = true;
            continue;
          }
        }

        // Strategy 2: upstream gap collapsing -- move closer to dependencies.
        const std::vector<NeighborRef>& ins = neighbors(adj.incoming, nodeId);
        if (!ins.empty()) {
          int maxDepLevel = -1;
          bool hasValidDeps = false;
          for (const NeighborRef& in : ins) {
            const auto it = layers.find(in.other);
            if (it != layers.end()) {
              maxDepLevel = std::max(maxDepLevel, it->second);
              hasValidDeps = true;
            }
          }
          if (hasValidDeps && maxDepLevel >= 0) {
            const int optimalLevel = maxDepLevel + 1;
            if (level < optimalLevel - 1 &&
                !hasDownstreamInNextLevel(nodeId, optimalLevel, layers, adj)) {
              layers[nodeId] = std::min(optimalLevel, level + 2);
              changed = true;
            }
          }
        }
      }

      // Strategy 3: downstream gap collapsing -- push nodes toward consumers.
      // Applies to all nodes including level-0 roots.
      const std::vector<NeighborRef>& outs = neighbors(adj.outgoing, nodeId);
      if (!outs.empty()) {
        int minConsumerLevel = std::numeric_limits<int>::max();
        bool hasValidConsumers = false;
        for (const NeighborRef& out : outs) {
          const auto it = layers.find(out.other);
          if (it != layers.end()) {
            minConsumerLevel = std::min(minConsumerLevel, it->second);
            hasValidConsumers = true;
          }
        }
        const int currentLevel = layers[nodeId];
        if (hasValidConsumers && minConsumerLevel > currentLevel + 2) {
          const int targetLevel = minConsumerLevel - 1;
          bool canMove = true;
          for (const NeighborRef& in : neighbors(adj.incoming, nodeId)) {
            const auto it = layers.find(in.other);
            if (it != layers.end() && it->second >= targetLevel) {
              canMove = false;
              break;
            }
          }
          if (canMove) {
            layers[nodeId] = targetLevel;
            changed = true;
          }
        }
      }
    }
  }

  // Final pass: source nodes (no inputs) still gapped from their nearest consumer
  // get moved adjacent to it.
  for (const std::string& nodeId : component) {
    if (!neighbors(adj.incoming, nodeId).empty()) {
      continue;
    }
    const int level = layers[nodeId];
    int minConsumerLevel = std::numeric_limits<int>::max();
    for (const NeighborRef& out : neighbors(adj.outgoing, nodeId)) {
      const auto it = layers.find(out.other);
      if (it != layers.end()) {
        minConsumerLevel = std::min(minConsumerLevel, it->second);
      }
    }
    if (minConsumerLevel != std::numeric_limits<int>::max() && minConsumerLevel > level + 1) {
      layers[nodeId] = minConsumerLevel - 1;
    }
  }
}

// Ideal top-edge Y for a node, weighting connected pin positions and blending
// toward center alignment to keep chains straight.
double calculateIdealVerticalPosition(Ctx& ctx, const std::string& id) {
  const NodeData& node = nodeOf(ctx, id);
  const double halfHeight = node.size[1] * 0.5;

  const std::vector<NeighborRef>& ins = neighbors(ctx.adj.incoming, id);
  const std::vector<NeighborRef>& outs = neighbors(ctx.adj.outgoing, id);
  const auto distinctCount = [](const std::vector<NeighborRef>& refs) {
    std::unordered_set<std::string> targets;
    for (const NeighborRef& r : refs) {
      targets.insert(r.other);
    }
    return targets.size();
  };
  const bool isLinear = distinctCount(ins) <= 1 && distinctCount(outs) <= 1;
  const double centerBias = isLinear ? ctx.p.linearCenterBias : ctx.p.centerBias;

  double totalWeight = 0.0;
  double weightedSum = 0.0;

  for (const NeighborRef& in : ins) {
    const auto it = ctx.nodes.find(in.other);
    if (it == ctx.nodes.end()) {
      continue;
    }
    const NodeData& dep = it->second;
    const double pinY = dep.position[1] + pinCenterYLocal(dep, in.otherPin, /*isInput=*/false);
    const double ourPinOffset = pinCenterYLocal(node, in.ownPin, /*isInput=*/true);
    const double depCenter = dep.position[1] + dep.size[1] * 0.5;
    const double targetY = pinY + centerBias * (depCenter - pinY);
    const double offset = ourPinOffset + centerBias * (halfHeight - ourPinOffset);
    weightedSum += (targetY - offset) * ctx.p.inputWeight;
    totalWeight += ctx.p.inputWeight;
  }
  for (const NeighborRef& out : outs) {
    const auto it = ctx.nodes.find(out.other);
    if (it == ctx.nodes.end()) {
      continue;
    }
    const NodeData& dep = it->second;
    const double pinY = dep.position[1] + pinCenterYLocal(dep, out.otherPin, /*isInput=*/true);
    const double ourPinOffset = pinCenterYLocal(node, out.ownPin, /*isInput=*/false);
    const double depCenter = dep.position[1] + dep.size[1] * 0.5;
    const double targetY = pinY + centerBias * (depCenter - pinY);
    const double offset = ourPinOffset + centerBias * (halfHeight - ourPinOffset);
    weightedSum += (targetY - offset) * ctx.p.outputWeight;
    totalWeight += ctx.p.outputWeight;
  }

  return totalWeight > 0.0 ? weightedSum / totalWeight : 0.0;
}

// Group nodes in a layer that share an upstream dependency or downstream
// consumer, so related nodes stay vertically adjacent.
std::vector<NodeCluster> createClusters(Ctx& ctx, const std::vector<std::string>& layer) {
  std::vector<NodeCluster> clusters;
  std::unordered_set<std::string> processed;
  for (const std::string& seedId : layer) {
    if (processed.count(seedId)) {
      continue;
    }
    NodeCluster cluster;
    std::queue<std::string> toProcess;
    toProcess.push(seedId);
    while (!toProcess.empty()) {
      const std::string current = toProcess.front();
      toProcess.pop();
      if (processed.count(current)) {
        continue;
      }
      processed.insert(current);
      cluster.nodes.push_back(current);
      cluster.totalHeight += nodeOf(ctx, current).size[1];

      const std::vector<NeighborRef>& currentIn = neighbors(ctx.adj.incoming, current);
      const std::vector<NeighborRef>& currentOut = neighbors(ctx.adj.outgoing, current);
      for (const std::string& other : layer) {
        if (processed.count(other)) {
          continue;
        }
        bool shouldCluster = false;
        for (const NeighborRef& a : currentIn) {
          for (const NeighborRef& b : neighbors(ctx.adj.incoming, other)) {
            if (a.other == b.other) {
              shouldCluster = true;
              break;
            }
          }
          if (shouldCluster) {
            break;
          }
        }
        if (!shouldCluster) {
          for (const NeighborRef& a : currentOut) {
            for (const NeighborRef& b : neighbors(ctx.adj.outgoing, other)) {
              if (a.other == b.other) {
                shouldCluster = true;
                break;
              }
            }
            if (shouldCluster) {
              break;
            }
          }
        }
        if (shouldCluster) {
          toProcess.push(other);
        }
      }
    }
    clusters.push_back(std::move(cluster));
  }
  return clusters;
}

double calculateOptimalClusterY(Ctx& ctx, const NodeCluster& cluster) {
  double totalWeightedY = 0.0;
  double totalWeight = 0.0;
  for (const std::string& nodeId : cluster.nodes) {
    const NodeData& node = nodeOf(ctx, nodeId);
    const double idealTopEdge = calculateIdealVerticalPosition(ctx, nodeId);
    const double idealCenter = idealTopEdge + node.size[1] * 0.5;
    const double weight = 1.0 + static_cast<double>(neighbors(ctx.adj.incoming, nodeId).size()) +
        static_cast<double>(neighbors(ctx.adj.outgoing, nodeId).size());
    totalWeightedY += idealCenter * weight;
    totalWeight += weight;
  }
  return totalWeight > 0.0 ? totalWeightedY / totalWeight : 0.0;
}

void resolveClusterConflicts(std::vector<NodeCluster>& clusters, double minSpacing) {
  if (clusters.size() <= 1) {
    return;
  }
  std::sort(clusters.begin(), clusters.end(), [](const NodeCluster& a, const NodeCluster& b) {
    return a.centroidY < b.centroidY;
  });

  double meanBefore = 0.0;
  for (const NodeCluster& c : clusters) {
    meanBefore += c.centroidY;
  }
  meanBefore /= static_cast<double>(clusters.size());

  for (std::size_t i = 1; i < clusters.size(); ++i) {
    const double previousBottom = clusters[i - 1].centroidY + clusters[i - 1].totalHeight * 0.5;
    const double currentTop = clusters[i].centroidY - clusters[i].totalHeight * 0.5;
    if (currentTop < previousBottom + minSpacing) {
      clusters[i].centroidY = previousBottom + minSpacing + clusters[i].totalHeight * 0.5;
    }
  }

  double meanAfter = 0.0;
  for (const NodeCluster& c : clusters) {
    meanAfter += c.centroidY;
  }
  meanAfter /= static_cast<double>(clusters.size());

  const double drift = meanAfter - meanBefore;
  for (NodeCluster& c : clusters) {
    c.centroidY -= drift;
  }
}

// Seed Y positions with a tight per-level clustering so the iterative coordinate
// solver starts from a reasonable arrangement.
void tightVerticalClustering(Ctx& ctx, std::map<int, std::vector<std::string>>& levelGroups) {
  for (auto& [level, nodesInLevel] : levelGroups) {
    if (nodesInLevel.size() <= 1) {
      continue;
    }
    std::vector<NodeCluster> clusters = createClusters(ctx, nodesInLevel);
    for (NodeCluster& cluster : clusters) {
      cluster.centroidY = calculateOptimalClusterY(ctx, cluster);
    }
    resolveClusterConflicts(clusters, ctx.p.tightClusterMinSpacing);
    for (const NodeCluster& cluster : clusters) {
      double currentY = cluster.centroidY - cluster.totalHeight * 0.5;
      for (const std::string& nodeId : cluster.nodes) {
        NodeData& cnode = nodeOf(ctx, nodeId);
        cnode.position = Vec2d(cnode.position[0], currentY);
        currentY += cnode.size[1] + ctx.p.tightClusterGap;
      }
    }
  }
}

double getBarycenter(
    Ctx& ctx,
    const std::string& nodeId,
    const std::vector<std::string>& referenceLayer,
    bool useIncoming) {
  std::unordered_map<std::string, int> positions;
  for (int i = 0; i < static_cast<int>(referenceLayer.size()); ++i) {
    positions[referenceLayer[i]] = i;
  }

  double totalPosition = 0.0;
  int connectionCount = 0;
  const std::vector<NeighborRef>& refs =
      useIncoming ? neighbors(ctx.adj.incoming, nodeId) : neighbors(ctx.adj.outgoing, nodeId);
  for (const NeighborRef& r : refs) {
    const auto posIt = positions.find(r.other);
    if (posIt == positions.end()) {
      continue;
    }
    const auto nodeIt = ctx.nodes.find(r.other);
    if (nodeIt == ctx.nodes.end()) {
      continue;
    }
    // Reference pin is the connected node's output when looking upstream, its
    // input when looking downstream.
    const double frac = pinFraction(nodeIt->second, r.otherPin, /*isInput=*/!useIncoming);
    totalPosition += static_cast<double>(posIt->second) + frac;
    connectionCount++;
  }
  return connectionCount > 0 ? totalPosition / connectionCount : 0.0;
}

void barycenterSort(
    Ctx& ctx,
    std::vector<std::string>& layer,
    const std::vector<std::string>& referenceLayer,
    bool useIncoming) {
  std::vector<std::pair<std::string, double>> barycenters;
  barycenters.reserve(layer.size());
  for (const std::string& nodeId : layer) {
    barycenters.emplace_back(nodeId, getBarycenter(ctx, nodeId, referenceLayer, useIncoming));
  }
  std::stable_sort(barycenters.begin(), barycenters.end(), [](const auto& a, const auto& b) {
    return a.second < b.second;
  });
  layer.clear();
  for (const auto& [nodeId, barycenter] : barycenters) {
    layer.push_back(nodeId);
  }
}

double calculateCrossings(
    Ctx& ctx,
    const std::vector<std::string>& layer1,
    const std::vector<std::string>& layer2) {
  std::unordered_map<std::string, int> pos2;
  for (int i = 0; i < static_cast<int>(layer2.size()); ++i) {
    pos2[layer2[i]] = i;
  }

  double crossings = 0.0;
  for (std::size_t i = 0; i < layer1.size(); ++i) {
    for (const NeighborRef& link1 : neighbors(ctx.adj.outgoing, layer1[i])) {
      const auto it1 = pos2.find(link1.other);
      if (it1 == pos2.end()) {
        continue;
      }
      for (std::size_t j = i + 1; j < layer1.size(); ++j) {
        for (const NeighborRef& link2 : neighbors(ctx.adj.outgoing, layer1[j])) {
          const auto it2 = pos2.find(link2.other);
          if (it2 == pos2.end()) {
            continue;
          }
          if (it1->second > it2->second) {
            crossings += 1.0;
          }
        }
      }
    }
  }
  return crossings;
}

void minimizeCrossings(Ctx& ctx, std::map<int, std::vector<std::string>>& levelGroups) {
  constexpr int kMaxIterations = 5;
  int maxLevel = 0;
  for (const auto& [level, nodesInLevel] : levelGroups) {
    maxLevel = std::max(maxLevel, level);
  }

  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    bool improved = false;

    for (int level = 1; level <= maxLevel; ++level) {
      if (levelGroups[level].empty() || levelGroups[level - 1].empty()) {
        continue;
      }
      std::vector<std::string>& currentLayer = levelGroups[level];
      const std::vector<std::string>& previousLayer = levelGroups[level - 1];
      const double oldCrossings = calculateCrossings(ctx, previousLayer, currentLayer);
      barycenterSort(ctx, currentLayer, previousLayer, /*useIncoming=*/true);
      const double newCrossings = calculateCrossings(ctx, previousLayer, currentLayer);
      if (newCrossings < oldCrossings) {
        improved = true;
      }
    }

    for (int level = maxLevel - 1; level >= 0; --level) {
      if (levelGroups[level].empty() || levelGroups[level + 1].empty()) {
        continue;
      }
      std::vector<std::string>& currentLayer = levelGroups[level];
      const std::vector<std::string>& nextLayer = levelGroups[level + 1];
      const double oldCrossings = calculateCrossings(ctx, currentLayer, nextLayer);
      barycenterSort(ctx, currentLayer, nextLayer, /*useIncoming=*/false);
      const double newCrossings = calculateCrossings(ctx, currentLayer, nextLayer);
      if (newCrossings < oldCrossings) {
        improved = true;
      }
    }

    if (!improved) {
      break;
    }
  }
}

void assignCoordinates(Ctx& ctx, std::map<int, std::vector<std::string>>& levelGroups) {
  int maxLevel = 0;
  std::vector<std::string> allInComponent;
  for (const auto& [level, nodesInLevel] : levelGroups) {
    maxLevel = std::max(maxLevel, level);
    for (const std::string& id : nodesInLevel) {
      allInComponent.push_back(id);
    }
  }

  // X position per column from the widest node in that column.
  std::unordered_map<int, double> columnX;
  double currentX = 0.0;
  for (int level = 0; level <= maxLevel; ++level) {
    columnX[level] = currentX;
    double maxWidth = 0.0;
    const auto it = levelGroups.find(level);
    if (it != levelGroups.end()) {
      for (const std::string& nodeId : it->second) {
        maxWidth = std::max(maxWidth, nodeOf(ctx, nodeId).size[0]);
      }
    }
    currentX += maxWidth + ctx.p.columnGap;
  }

  std::unordered_map<int, std::vector<NodeCluster>> levelClusters;
  for (const auto& [level, nodesInLevel] : levelGroups) {
    if (!nodesInLevel.empty()) {
      levelClusters[level] = createClusters(ctx, nodesInLevel);
    }
  }

  const auto positionLevel = [&](int level) {
    const auto groupIt = levelGroups.find(level);
    if (groupIt == levelGroups.end() || groupIt->second.empty()) {
      return;
    }
    const double xPos = columnX[level];
    std::vector<NodeCluster>& clusters = levelClusters[level];

    for (NodeCluster& cluster : clusters) {
      cluster.centroidY = calculateOptimalClusterY(ctx, cluster);
      cluster.totalHeight = 0.0;
      for (const std::string& nodeId : cluster.nodes) {
        cluster.totalHeight += nodeOf(ctx, nodeId).size[1];
      }
      cluster.totalHeight += static_cast<double>(cluster.nodes.size() - 1) * ctx.p.intraClusterGap;
    }

    // Resolve inter-cluster overlaps in Y-sorted order while preserving the
    // crossing-minimization order for placement.
    std::vector<std::size_t> sortedIndices(clusters.size());
    for (std::size_t i = 0; i < sortedIndices.size(); ++i) {
      sortedIndices[i] = i;
    }
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](std::size_t a, std::size_t b) {
      return clusters[a].centroidY < clusters[b].centroidY;
    });
    for (std::size_t k = 1; k < sortedIndices.size(); ++k) {
      NodeCluster& prev = clusters[sortedIndices[k - 1]];
      NodeCluster& curr = clusters[sortedIndices[k]];
      const double prevBottom = prev.centroidY + prev.totalHeight * 0.5;
      const double currTop = curr.centroidY - curr.totalHeight * 0.5;
      if (currTop < prevBottom + ctx.p.interClusterGap) {
        curr.centroidY = prevBottom + ctx.p.interClusterGap + curr.totalHeight * 0.5;
      }
    }

    // Re-center to avoid drift.
    if (!clusters.empty()) {
      double meanIdeal = 0.0;
      double meanActual = 0.0;
      for (const NodeCluster& cluster : clusters) {
        meanActual += cluster.centroidY;
        meanIdeal += calculateOptimalClusterY(ctx, cluster);
      }
      meanIdeal /= static_cast<double>(clusters.size());
      meanActual /= static_cast<double>(clusters.size());
      const double drift = meanActual - meanIdeal;
      for (NodeCluster& cluster : clusters) {
        cluster.centroidY -= drift;
      }
    }

    for (const NodeCluster& cluster : clusters) {
      double nodeY = cluster.centroidY - cluster.totalHeight * 0.5;
      for (const std::string& nodeId : cluster.nodes) {
        NodeData& node = nodeOf(ctx, nodeId);
        node.position = Vec2d(xPos, nodeY);
        nodeY += node.size[1] + ctx.p.intraClusterGap;
      }
    }
  };

  constexpr int kMaxCoordIterations = 20;
  for (int iteration = 0; iteration < kMaxCoordIterations; ++iteration) {
    std::unordered_map<std::string, double> prevY;
    prevY.reserve(allInComponent.size());
    for (const std::string& id : allInComponent) {
      prevY[id] = nodeOf(ctx, id).position[1];
    }

    for (int level = 0; level <= maxLevel; ++level) {
      positionLevel(level);
    }
    for (int level = maxLevel - 1; level >= 0; --level) {
      positionLevel(level);
    }

    double maxDelta = 0.0;
    for (const std::string& id : allInComponent) {
      maxDelta = std::max(maxDelta, std::abs(nodeOf(ctx, id).position[1] - prevY[id]));
    }
    if (maxDelta < 0.5) {
      break;
    }
  }
}

void applyJitter(Ctx& ctx, const std::vector<std::string>& component) {
  if (ctx.p.jitterFractionX == 0.0 && ctx.p.jitterFractionY == 0.0) {
    return;
  }
  for (const std::string& id : component) {
    const uint64_t hash = stableHash(id);
    const double jitterX =
        (static_cast<double>(hash % 1000) / 500.0 - 1.0) * ctx.p.columnGap * ctx.p.jitterFractionX;
    const double jitterY = (static_cast<double>((hash / 1000) % 1000) / 500.0 - 1.0) *
        ctx.p.interClusterGap * ctx.p.jitterFractionY;
    NodeData& node = nodeOf(ctx, id);
    node.position = Vec2d(node.position) + Vec2d(jitterX, jitterY);
  }
}

// Stack a component below everything laid out so far. Returns the Y at which the
// next component should start.
double stackComponent(Ctx& ctx, const std::vector<std::string>& component, double componentY) {
  double minY = std::numeric_limits<double>::max();
  double maxY = std::numeric_limits<double>::lowest();
  for (const std::string& id : component) {
    const NodeData& node = nodeOf(ctx, id);
    minY = std::min(minY, node.position[1]);
    maxY = std::max(maxY, node.position[1] + node.size[1]);
  }
  const double offsetY = componentY - minY;
  for (const std::string& id : component) {
    NodeData& snode = nodeOf(ctx, id);
    snode.position = Vec2d(snode.position[0], snode.position[1] + offsetY);
  }
  return maxY + offsetY + ctx.p.componentGap;
}

} // namespace

void layoutGraph(GraphModel& graph, const LayoutParams& params) {
  if (graph.nodes.empty()) {
    return;
  }

  const Adjacency adj = buildDataAdjacency(graph);
  Ctx ctx{graph.nodes, adj, params};

  const std::vector<std::string> sortedIds = sortedNodeIds(graph);
  const std::vector<std::vector<std::string>> components = findConnectedComponents(sortedIds, adj);

  double componentY = 0.0;
  for (const std::vector<std::string>& component : components) {
    std::unordered_map<std::string, int> layers = assignLayers(component, adj);
    compactLayers(component, layers, adj);

    std::map<int, std::vector<std::string>> levelGroups;
    for (const std::string& id : component) {
      levelGroups[layers.at(id)].push_back(id);
    }

    tightVerticalClustering(ctx, levelGroups);
    minimizeCrossings(ctx, levelGroups);
    assignCoordinates(ctx, levelGroups);
    applyJitter(ctx, component);
    componentY = stackComponent(ctx, component, componentY);
  }
}

} // namespace noodles
