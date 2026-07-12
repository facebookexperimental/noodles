// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "core/GraphLayout.h"
#include "core/Math.h"
#include "core/NodeData.h"

#include <map>
#include <string>
#include <utility>

namespace noodles {
namespace {

NodeData makeNode(const std::string& id, double width = 200.0, double height = 100.0) {
  NodeData node;
  node.id = id;
  node.name = id;
  node.size = Vec2d(width, height);
  return node;
}

LinkData
makeLink(const std::string& source, const std::string& target, bool isRelationship = false) {
  LinkData link;
  link.sourceNodeId = source;
  link.sourcePort = "out";
  link.targetNodeId = target;
  link.targetPort = "in";
  link.isRelationship = isRelationship;
  return link;
}

// Params with jitter disabled so structural assertions are not perturbed by the
// organic offset. Jitter itself is covered by LayoutIsDeterministic.
LayoutParams noJitter() {
  LayoutParams params;
  params.jitterFractionX = 0.0;
  params.jitterFractionY = 0.0;
  return params;
}

std::map<std::string, std::pair<double, double>> positionsOf(const GraphModel& graph) {
  std::map<std::string, std::pair<double, double>> result;
  for (const auto& [id, node] : graph.nodes) {
    result[id] = {node.position[0], node.position[1]};
  }
  return result;
}

double xOf(const GraphModel& graph, const std::string& id) {
  return graph.nodes.at(id).position[0];
}

TEST(GraphLayoutTest, EmptyGraphIsNoOp) {
  GraphModel graph;
  layoutGraph(graph, noJitter());
  EXPECT_TRUE(graph.nodes.empty());
}

TEST(GraphLayoutTest, DataFlowChainRanksLeftToRight) {
  // A -> B -> C over data links: nodes land in strictly increasing columns.
  GraphModel graph;
  graph.nodes.emplace("A", makeNode("A"));
  graph.nodes.emplace("B", makeNode("B"));
  graph.nodes.emplace("C", makeNode("C"));
  graph.links = {makeLink("A", "B"), makeLink("B", "C")};

  layoutGraph(graph, noJitter());

  EXPECT_LT(xOf(graph, "A"), xOf(graph, "B"));
  EXPECT_LT(xOf(graph, "B"), xOf(graph, "C"));
}

TEST(GraphLayoutTest, DiamondPlacesSourceAndSinkInOuterColumns) {
  // A fans out to B and C, which both feed D. B and C share a column between the
  // source A and sink D.
  GraphModel graph;
  for (const char* id : {"A", "B", "C", "D"}) {
    graph.nodes.emplace(id, makeNode(id));
  }
  graph.links = {makeLink("A", "B"), makeLink("A", "C"), makeLink("B", "D"), makeLink("C", "D")};

  layoutGraph(graph, noJitter());

  EXPECT_LT(xOf(graph, "A"), xOf(graph, "B"));
  EXPECT_LT(xOf(graph, "A"), xOf(graph, "C"));
  EXPECT_DOUBLE_EQ(xOf(graph, "B"), xOf(graph, "C")); // same rank -> same column
  EXPECT_LT(xOf(graph, "B"), xOf(graph, "D"));
  EXPECT_LT(xOf(graph, "C"), xOf(graph, "D"));
}

TEST(GraphLayoutTest, RelationshipEdgeDoesNotAffectLayout) {
  // THE invariant: relationship links are excluded from every structural phase,
  // so adding one that WOULD merge two components / create a back-edge if it were
  // ranked must leave every node's position byte-for-byte unchanged.
  const auto buildBase = []() {
    GraphModel graph;
    for (const char* id : {"A", "B", "C", "D"}) {
      graph.nodes.emplace(id, makeNode(id));
    }
    // Two independent data components: A -> B and C -> D.
    graph.links = {makeLink("A", "B"), makeLink("C", "D")};
    return graph;
  };

  GraphModel dataOnly = buildBase();
  layoutGraph(dataOnly, noJitter());

  GraphModel withRelationship = buildBase();
  // A relationship edge bridging the two components, plus a back-edge: if either
  // were treated as a data edge it would change ranks / merge components.
  withRelationship.links.push_back(makeLink("B", "C", /*isRelationship=*/true));
  withRelationship.links.push_back(makeLink("D", "A", /*isRelationship=*/true));
  layoutGraph(withRelationship, noJitter());

  EXPECT_EQ(positionsOf(dataOnly), positionsOf(withRelationship));
}

TEST(GraphLayoutTest, RelationshipOnlyNodesStackVerticallyNotChained) {
  // Two nodes joined ONLY by a relationship link are NOT data-connected, so they
  // form separate components stacked vertically (same column, different rows)
  // rather than being placed left-to-right as a chain.
  GraphModel graph;
  graph.nodes.emplace("A", makeNode("A"));
  graph.nodes.emplace("B", makeNode("B"));
  graph.links = {makeLink("A", "B", /*isRelationship=*/true)};

  layoutGraph(graph, noJitter());

  EXPECT_DOUBLE_EQ(graph.nodes.at("A").position[0], graph.nodes.at("B").position[0]);
  EXPECT_NE(graph.nodes.at("A").position[1], graph.nodes.at("B").position[1]);
}

TEST(GraphLayoutTest, LayoutIsDeterministic) {
  // Same graph laid out twice (with default jitter on) yields identical positions
  // -- sorted iteration + a stable jitter hash remove unordered_map / std::hash
  // nondeterminism.
  const auto build = []() {
    GraphModel graph;
    for (const char* id : {"n0", "n1", "n2", "n3", "n4"}) {
      graph.nodes.emplace(id, makeNode(id));
    }
    graph.links = {
        makeLink("n0", "n1"),
        makeLink("n0", "n2"),
        makeLink("n1", "n3"),
        makeLink("n2", "n3"),
        makeLink("n3", "n4")};
    return graph;
  };

  GraphModel first = build();
  GraphModel second = build();
  layoutGraph(first); // default params: jitter on
  layoutGraph(second);

  EXPECT_EQ(positionsOf(first), positionsOf(second));
}

} // namespace
} // namespace noodles
