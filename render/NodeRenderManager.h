// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_NODE_RENDER_MANAGER_H
#define NOODLES_RENDER_NODE_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/RenderConfig.h"
#include "core/api.h"
#include "core/pragmas.h"
#include "render/FontAtlas.h"
#include "render/NodeTransformFrame.h"
#include "render/NodeVertexCache.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API RenderBatch {
  GLuint vao = 0;
  GLuint vbo = 0;
  uint32_t cachedGeneration = 0;
  int cachedVertexCount = 0;
};

/// Input to assembleNodeQuadBuffer: one node's base-frame quad geometry plus the
/// transform-texture slot stamped into each vertex's nodeIndex. Background depth
/// (z) is already baked at generation, so moves never re-assemble.
struct NodeQuadAssemblyItem {
  const std::vector<NodeVertex>* vertices = nullptr;
  float nodeIndex = -1.0f;
};

// Interaction state the port-highlight producer needs but does not own: the
// hovered port and the in-progress link drag source. Mirrors the scalars the
// Python _paintPortHighlights pass reads from GraphView.
struct PortHighlightState {
  bool hasHover = false;
  std::string hoveredNodeId;
  std::string hoveredPort;
  bool hoveredIsOutput = false;
  bool draggingLink = false;
  std::string dragSourceNodeId;
  std::string dragSourcePort;
  bool dragSourceIsOutput = false;

  bool operator==(const PortHighlightState& o) const {
    return hasHover == o.hasHover && hoveredNodeId == o.hoveredNodeId &&
        hoveredPort == o.hoveredPort && hoveredIsOutput == o.hoveredIsOutput &&
        draggingLink == o.draggingLink && dragSourceNodeId == o.dragSourceNodeId &&
        dragSourcePort == o.dragSourcePort && dragSourceIsOutput == o.dragSourceIsOutput;
  }
  bool operator!=(const PortHighlightState& o) const {
    return !(*this == o);
  }
};

// Port-highlight geometry, partitioned exactly as the Python pass partitions it:
// `regular` (port circles) draws through the node shader like renderNodes;
// `relationship` (authoring triangles) draws as a plain vertex batch.
struct PortHighlightVertices {
  std::vector<NodeVertex> regular;
  std::vector<NodeVertex> relationship;
};

// One logical port in the graph, yielded by forEachPort: its live (drag-shifted)
// side-correct circle center, side, connection state, and whether it is backed by
// a USD relationship. The port-highlight producer and the port hit-test both
// consume these glyphs, so the drawn circle and its hit region can never drift.
struct PortGlyph {
  const std::string& nodeId;
  const NodeData& node;
  std::string pinName;
  bool isOutput = false;
  // Live, side-correct port-circle center (snapshot center + the node's drag
  // shift). The producer draws the circle here; the hit-test distance-tests here.
  double centerX = 0.0;
  double centerY = 0.0;
  // The node's live drag shift, exposed so a consumer can derive other live
  // coordinates (e.g. the right edge a relationship triangle draws on).
  double shiftX = 0.0;
  double shiftY = 0.0;
  bool isConnected = false;
  bool isRelationship = false;
};

class NOODLES_API NodeRenderManager {
 public:
  NodeRenderManager();
  ~NodeRenderManager();

  NodeRenderManager(const NodeRenderManager&) = delete;
  NodeRenderManager& operator=(const NodeRenderManager&) = delete;
  NodeRenderManager(NodeRenderManager&&) = delete;
  NodeRenderManager& operator=(NodeRenderManager&&) = delete;

  void initialize(ShaderLibrary* shaders);

  // The shared node-local coordinate frame (base positions + move offsets +
  // transform texture) used by renderNodesFromGraph. Non-owning; the same frame
  // is shared with TextRenderManager so text and quads move together.
  void setTransformFrame(NodeTransformFrame* frame) {
    transformFrame_ = frame;
  }

  NodeVertexCache& vertexCache() {
    return vertexCache_;
  }
  const NodeVertexCache& vertexCache() const {
    return vertexCache_;
  }

  void renderNodes(
      const std::vector<NodeVertex>& vertices,
      uint32_t strokeColor,
      uint32_t generation,
      bool selectionChanged,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& innerStrokeColor);

  // C++-owned node-quad path: generate background quads from the authoritative
  // GraphModel.nodes snapshot and draw them. Geometry is regenerated only when
  // contentChanged || needsNodeRebuild(); moves are applied via the shared
  // transform frame (NodeTransformFrame::translate) so dragging never re-gens.
  void renderNodesFromGraph(
      GraphModel& graph,
      const RenderConfig& config,
      const FontAtlas& fontAtlas,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& innerStrokeColor,
      bool contentChanged);

  // Drop the cached quad buffer and force a full rebuild on the next render
  // (called when the graph is (re)loaded or re-laid-out). The shared node
  // positions live in NodeTransformFrame and are reset there.
  void resetNodeQuadCaches();

  void markNodeQuadDirty(bool needsRebuild = false);

  // True when the next renderNodesFromGraph will regenerate geometry.
  bool needsNodeRebuild() const {
    return nodeQuadNeedsRebuild_;
  }

  // Pure (no GL / FontAtlas) for unit-testability: concatenate per-node base-
  // frame quad blobs, stamping each vertex's transform-texture slot.
  static std::vector<NodeVertex> assembleNodeQuadBuffer(
      const std::vector<NodeQuadAssemblyItem>& items);

  void renderPortHighlight(
      float portX,
      float portY,
      float portW,
      float portH,
      float depth,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& highlightColor);

  void renderEndpointCircle(
      float cx,
      float cy,
      float radius,
      float depth,
      const float* projection4x4,
      const std::array<float, 4>& fillColor,
      const std::array<float, 4>& strokeColor,
      float strokeWidth);

  void renderEndpointTriangle(
      float x0,
      float y0,
      float x1,
      float y1,
      float x2,
      float y2,
      float depth,
      const float* projection4x4,
      const std::array<float, 4>& color);

  void invalidateAll();
  void invalidateNode(const std::string& nodeId);
  void clearCache();
  void cleanup();

  static std::vector<NodeVertex> buildCircleEndpointVertices(
      float cx,
      float cy,
      float radius,
      float depth,
      const std::array<float, 4>& fillColor,
      const std::array<float, 4>& strokeColor,
      float strokeWidth);

  static std::vector<NodeVertex> buildTriangleEndpointVertices(
      float x0,
      float y0,
      float x1,
      float y1,
      float x2,
      float y2,
      float depth,
      const std::array<float, 4>& color);

  static std::vector<NodeVertex> buildPortQuadVertices(
      float portX,
      float portY,
      float portW,
      float portH,
      float depth,
      const std::array<float, 4>& color);

  // The single port-enumeration walk shared by the highlight producer and the
  // hit-test: invokes `visit` once per logical port on every node (normal /
  // folded-header aggregate / dual / output-dual / synthetic relationship /
  // title-collapsed aggregate), with each port's live side-correct center already
  // resolved from the cached per-pin row centers (layoutInput/OutputCenterY), the
  // dual-pin sets, and the synthetic-relationship endpoints. transformFrame
  // (optional) supplies each node's live move offset so ports follow a drag; pass
  // nullptr for absolute positions (e.g. tests). One source of truth for port
  // position/side/kind, so the drawn circle and its hit region never diverge.
  static void forEachPort(
      const GraphModel& graph,
      const NodeTransformFrame* transformFrame,
      const std::function<void(const PortGlyph&)>& visit);

  // Pure (no GL / FontAtlas) port-highlight producer: walks GraphModel.nodes (via
  // forEachPort) and builds the port-circle + relationship-triangle vertices the
  // highlight pass draws, reading the C++ connection index (isPortConnected /
  // isFoldedHeaderConnected), synthetic relationship ports, the cached per-pin
  // row centers (layoutInput/OutputCenterY), and the port colors on RenderConfig.
  // A faithful port of graphView._paintPortHighlights + _addPortVertices, so the
  // render path no longer assembles this geometry in Python every frame.
  // transformFrame (optional) supplies each node's live move offset so port
  // glyphs follow a drag; pass nullptr for absolute positions (e.g. tests).
  static PortHighlightVertices buildPortHighlightVertices(
      const GraphModel& graph,
      const RenderConfig& config,
      const PortHighlightState& state,
      const NodeTransformFrame* transformFrame = nullptr);

  // Result of portAtPoint: the closest port under a cursor (or found = false).
  struct PortHit {
    bool found = false;
    std::string nodeId;
    std::string portName;
    bool isOutput = false;
  };

  // Hit-test the cursor against every port the forEachPort walk produces (normal,
  // folded-header aggregate, dual / output-dual mirror, synthetic relationship,
  // title-collapsed aggregate), returning the closest within hitRadius (world
  // units) or {found = false}. Unlike the highlight producer this ignores the
  // connected/visibility gate — a disconnected pin is still a valid hover /
  // link-drag target — and resolves a collapsed node's aggregate handle to its
  // first display pin. transformFrame (optional) supplies each node's live move
  // offset so hit regions track a drag, matching the drawn circles; pass nullptr
  // for absolute positions (e.g. tests). Replaces the per-node
  // GraphNodeRenderer::getPortAtPoint plus the Python _findDualPinHover /
  // _findOutputDualPinHover / _findSyntheticRelationshipHover passes.
  static PortHit portAtPoint(
      const GraphModel& graph,
      const Vec2d& worldPos,
      double hitRadius,
      const NodeTransformFrame* transformFrame = nullptr);

  // Non-static wrapper that supplies the shared transform frame (set via
  // setTransformFrame), for the Python binding. The static portAtPoint stays pure
  // for unit tests.
  PortHit portAtPointFromGraph(const GraphModel& graph, const Vec2d& worldPos, double hitRadius)
      const {
    return portAtPoint(graph, worldPos, hitRadius, transformFrame_);
  }

  // FNV-1a hash over a port-highlight vertex batch's geometry. Used as the VBO
  // "generation" in renderPortHighlightsFromGraph so createOrUpdateVBO skips the
  // re-upload when the highlight geometry is unchanged (idle / pan / zoom);
  // hashing the produced geometry rather than the (many) inputs means the
  // change-gate can never go stale by missing an input. Pure; unit-tested.
  static uint32_t hashPortHighlightVertices(const std::vector<NodeVertex>& vertices);

  // C++-owned port-highlight path: build the geometry from the authoritative
  // GraphModel snapshot and draw it — port circles through the node shader, the
  // relationship triangles as a separate batch. Replaces the per-frame Python
  // _paintPortHighlights assembly + renderNodes/_drawNodeVertices calls.
  // geometryChanged: the caller's signal that node geometry / connections /
  // colors changed (or a node drag is in progress) this frame. The highlight
  // verts are rebuilt when it is true, when the hover / link-drag state changed
  // (compared internally), or on the first call; otherwise the cached verts are
  // redrawn from the persistent VBOs -- a pan / zoom / idle frame skips the
  // O(ports) build + hash entirely.
  void renderPortHighlightsFromGraph(
      const GraphModel& graph,
      const RenderConfig& config,
      const PortHighlightState& state,
      const float* projection4x4,
      float cornerRadius,
      bool geometryChanged);

 private:
  static void drawVerticesImmediate(
      const std::vector<NodeVertex>& vertices,
      GLSLProgram* shader,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& strokeColor);

  void createOrUpdateVBO(
      RenderBatch& batch,
      const std::vector<NodeVertex>& vertices,
      uint32_t generation,
      bool selectionChanged);
  void setupVAO(RenderBatch& batch, GLsizeiptr dataSize, const void* data);

  std::vector<NodeVertex> generateNodeQuadVertices(
      GraphModel& graph,
      const RenderConfig& config,
      const FontAtlas& fontAtlas);

  NodeVertexCache vertexCache_;
  std::unordered_map<uint32_t, RenderBatch> batches_;
  ShaderLibrary* shaders_ = nullptr;
  bool initialized_ = false;

  // --- C++-owned node-quad (from-graph) path ---
  NodeTransformFrame* transformFrame_ = nullptr; // shared, non-owning
  RenderBatch nodeQuadBatch_;
  std::vector<NodeVertex> nodeQuadVertexData_;
  bool nodeQuadNeedsRebuild_ = true;

  // --- C++-owned port-highlight cache ---
  // The highlight geometry only changes on a hover / link-drag state change
  // (detected by comparing lastPortHighlightState_) or when the caller signals
  // geometryChanged (node geometry / connections / colors / node drag). On
  // idle / pan / zoom frames the cached verts are redrawn from their persistent
  // VBOs without rebuilding or re-hashing them.
  PortHighlightVertices cachedPortHighlightVerts_;
  PortHighlightState lastPortHighlightState_;
  uint32_t cachedPortHighlightRegularHash_ = 0;
  uint32_t cachedPortHighlightRelationshipHash_ = 0;
  bool havePortHighlightCache_ = false;
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_NODE_RENDER_MANAGER_H
