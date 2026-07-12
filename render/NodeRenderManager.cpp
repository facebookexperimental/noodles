// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/NodeRenderManager.h"

#include "render/GraphNodeRenderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace noodles {

namespace {
uint8_t floatColorToByte(float v) {
  return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}
} // namespace

NodeRenderManager::NodeRenderManager() = default;

NodeRenderManager::~NodeRenderManager() {
  cleanup();
}

void NodeRenderManager::initialize(ShaderLibrary* shaders) {
  shaders_ = shaders;
  initialized_ = true;
}

void NodeRenderManager::renderNodes(
    const std::vector<NodeVertex>& vertices,
    uint32_t strokeColor,
    uint32_t generation,
    bool selectionChanged,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& innerStrokeColor) {
  if (!initialized_ || !shaders_ || vertices.empty()) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint radiusLoc = shader->getUniformLocation("uCornerRadius");
  if (radiusLoc >= 0) {
    glUniform1f(radiusLoc, cornerRadius);
  }

  GLint strokeLoc = shader->getUniformLocation("uInnerStrokeColor");
  if (strokeLoc >= 0) {
    glUniform4fv(strokeLoc, 1, innerStrokeColor.data());
  }

  auto& batch = batches_[strokeColor];
  createOrUpdateVBO(batch, vertices, generation, selectionChanged);
  if (batch.vao == 0) {
    // setupVAO bailed out because the GL context was unusable (e.g. mid stage
    // reopen on NVIDIA Windows).  Skip the draw — we'll retry next frame
    // once the driver is healthy.
    shader->release();
    return;
  }

  glBindVertexArray(batch.vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);

  shader->release();
}

std::vector<NodeVertex> NodeRenderManager::assembleNodeQuadBuffer(
    const std::vector<NodeQuadAssemblyItem>& items) {
  size_t total = 0;
  for (const auto& item : items) {
    if (item.vertices != nullptr) {
      total += item.vertices->size();
    }
  }

  std::vector<NodeVertex> buffer;
  buffer.reserve(total);
  for (const auto& item : items) {
    if (item.vertices == nullptr) {
      continue;
    }
    for (NodeVertex vertex : *item.vertices) {
      vertex.nodeIndex = item.nodeIndex;
      buffer.push_back(vertex);
    }
  }
  return buffer;
}

std::vector<NodeVertex> NodeRenderManager::generateNodeQuadVertices(
    GraphModel& graph,
    const RenderConfig& config,
    const FontAtlas& fontAtlas) {
  // The shared frame owns base positions / move offsets / slot assignment.
  transformFrame_->syncFromNodes(graph.nodes);

  // Back-to-front so a node's background quad sits behind higher-zOrder nodes
  // (same ordering the text path uses, so the layers stay in lockstep).
  auto sortedNodes = sortNodesByZOrder(graph.nodes);

  DefaultNodeRenderer defaultRenderer(config);
  GraffiNodeRenderer graffiRenderer(config);

  // Generate each node in its base frame; background depth (incl. the per-row
  // stripe/spacer sub-offsets) is baked here, the move offset is applied in the
  // shader via the transform texture.
  std::vector<std::vector<NodeVertex>> blobs;
  blobs.reserve(sortedNodes.size());
  std::vector<NodeQuadAssemblyItem> items;
  items.reserve(sortedNodes.size());
  for (auto& [nodeIdPtr, nodePtr] : sortedNodes) {
    NodeData& node = *nodePtr;
    Vec2d savedPos = node.position;
    node.position = transformFrame_->basePosition(*nodeIdPtr);

    // Mirror the OLD per-node renderer selection (createNodeRenderer): use the
    // node's own uiStyle, falling back to the global config style when it is
    // unset (legacy/virtual nodes leave uiStyle empty).
    const bool isGraffi =
        node.uiStyle == "graffi" || (node.uiStyle.empty() && config.nodeRendererType == "graffi");
    GraphNodeRenderer& renderer = isGraffi ? static_cast<GraphNodeRenderer&>(graffiRenderer)
                                           : static_cast<GraphNodeRenderer&>(defaultRenderer);
    auto depth = static_cast<float>(RenderConfig::backgroundDepth(node.zOrder));
    blobs.push_back(renderer.generateVertices(node, depth, fontAtlas));

    node.position = savedPos;
    // shaderIndex is -1.0 for nodes beyond the transform texture, so they fall
    // back to their base frame instead of a clamped edge texel's offset.
    items.push_back({&blobs.back(), transformFrame_->shaderIndex(*nodeIdPtr)});
  }

  nodeQuadVertexData_ = assembleNodeQuadBuffer(items);
  nodeQuadNeedsRebuild_ = false;
  return nodeQuadVertexData_;
}

void NodeRenderManager::renderNodesFromGraph(
    GraphModel& graph,
    const RenderConfig& config,
    const FontAtlas& fontAtlas,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& innerStrokeColor,
    bool contentChanged) {
  if (!initialized_ || !shaders_ || !transformFrame_ || graph.nodes.empty()) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  const bool justRebuilt = contentChanged || nodeQuadNeedsRebuild_;
  if (justRebuilt) {
    generateNodeQuadVertices(graph, config, fontAtlas);
  }
  if (nodeQuadVertexData_.empty()) {
    return;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }
  GLint radiusLoc = shader->getUniformLocation("uCornerRadius");
  if (radiusLoc >= 0) {
    glUniform1f(radiusLoc, cornerRadius);
  }
  GLint strokeLoc = shader->getUniformLocation("uInnerStrokeColor");
  if (strokeLoc >= 0) {
    glUniform4fv(strokeLoc, 1, innerStrokeColor.data());
  }

  // Move offsets live in the shared frame; it gates its own upload on dirty.
  transformFrame_->upload();
  transformFrame_->bindToShader(shader, /*textureUnit=*/0);

  // Only a rebuild changes the geometry, so move / idle / pan / zoom frames
  // reuse the VBO (createOrUpdateVBO re-uploads only when justRebuilt).
  createOrUpdateVBO(nodeQuadBatch_, nodeQuadVertexData_, /*generation=*/0, justRebuilt);
  if (nodeQuadBatch_.vao == 0) {
    shader->release();
    return;
  }

  glBindVertexArray(nodeQuadBatch_.vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(nodeQuadVertexData_.size()));
  glBindVertexArray(0);

  shader->release();
}

void NodeRenderManager::resetNodeQuadCaches() {
  nodeQuadVertexData_.clear();
  nodeQuadNeedsRebuild_ = true;
}

void NodeRenderManager::markNodeQuadDirty(bool needsRebuild) {
  if (needsRebuild) {
    nodeQuadNeedsRebuild_ = true;
  }
}

std::vector<NodeVertex> NodeRenderManager::buildPortQuadVertices(
    float portX,
    float portY,
    float portW,
    float portH,
    float depth,
    const std::array<float, 4>& color) {
  auto r = floatColorToByte(color[0]);
  auto g = floatColorToByte(color[1]);
  auto b = floatColorToByte(color[2]);
  auto a = floatColorToByte(color[3]);

  // Use selected = -1.0f sentinel to trigger per-vertex stroke color in the shader
  std::vector<NodeVertex> vertices;
  vertices.reserve(6);

  // Triangle 1: top-left, top-right, bottom-right
  vertices.emplace_back(
      portX, portY, depth, 0.0f, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW, portY, depth, portW, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW,
      portY + portH,
      depth,
      portW,
      portH,
      portW,
      portH,
      r,
      g,
      b,
      a,
      0.0f,
      r,
      g,
      b,
      a,
      -1.0f);

  // Triangle 2: top-left, bottom-right, bottom-left
  vertices.emplace_back(
      portX, portY, depth, 0.0f, 0.0f, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);
  vertices.emplace_back(
      portX + portW,
      portY + portH,
      depth,
      portW,
      portH,
      portW,
      portH,
      r,
      g,
      b,
      a,
      0.0f,
      r,
      g,
      b,
      a,
      -1.0f);
  vertices.emplace_back(
      portX, portY + portH, depth, 0.0f, portH, portW, portH, r, g, b, a, 0.0f, r, g, b, a, -1.0f);

  return vertices;
}

void NodeRenderManager::drawVerticesImmediate(
    const std::vector<NodeVertex>& vertices,
    GLSLProgram* shader,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& strokeColor) {
  auto packedData = NodeVertex::packBatch(vertices);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint radiusLoc = shader->getUniformLocation("uCornerRadius");
  if (radiusLoc >= 0) {
    glUniform1f(radiusLoc, cornerRadius);
  }

  GLint strokeLoc = shader->getUniformLocation("uInnerStrokeColor");
  if (strokeLoc >= 0) {
    glUniform4fv(strokeLoc, 1, strokeColor.data());
  }

  GLuint vao = 0;
  GLuint vbo = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(packedData.size()),
      packedData.data(),
      GL_STREAM_DRAW);

  NodeVertex::setupVertexAttribs();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);

  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &vao);

  shader->release();
}

void NodeRenderManager::renderPortHighlight(
    float portX,
    float portY,
    float portW,
    float portH,
    float depth,
    const float* projection4x4,
    float cornerRadius,
    const std::array<float, 4>& highlightColor) {
  if (!initialized_ || !shaders_) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices = buildPortQuadVertices(portX, portY, portW, portH, depth, highlightColor);
  drawVerticesImmediate(vertices, shader, projection4x4, cornerRadius, highlightColor);
}

std::vector<NodeVertex> NodeRenderManager::buildCircleEndpointVertices(
    float cx,
    float cy,
    float radius,
    float depth,
    const std::array<float, 4>& fillColor,
    const std::array<float, 4>& strokeColor,
    float strokeWidth) {
  strokeWidth = std::max(strokeWidth, 0.0f);

  float diameter = radius * 2.0f;
  float left = cx - radius;
  float top = cy - radius;

  auto fr = floatColorToByte(fillColor[0]);
  auto fg = floatColorToByte(fillColor[1]);
  auto fb = floatColorToByte(fillColor[2]);
  auto fa = floatColorToByte(fillColor[3]);

  auto sr = floatColorToByte(strokeColor[0]);
  auto sg = floatColorToByte(strokeColor[1]);
  auto sb = floatColorToByte(strokeColor[2]);
  auto sa = floatColorToByte(strokeColor[3]);

  // clang-format off
  return {
      {left,            top,            depth, 0.0f,     0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top,            depth, diameter, 0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top + diameter, depth, diameter, diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left,            top,            depth, 0.0f,     0.0f,     diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left + diameter, top + diameter, depth, diameter, diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
      {left,            top + diameter, depth, 0.0f,     diameter, diameter, diameter, fr, fg, fb, fa, strokeWidth, sr, sg, sb, sa, -1.0f},
  };
  // clang-format on
}

std::vector<NodeVertex> NodeRenderManager::buildTriangleEndpointVertices(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float depth,
    const std::array<float, 4>& color) {
  float minX = std::min({x0, x1, x2});
  float maxX = std::max({x0, x1, x2});
  float minY = std::min({y0, y1, y2});
  float maxY = std::max({y0, y1, y2});
  float w = std::max(maxX - minX, 1.0f);
  float h = std::max(maxY - minY, 1.0f);

  auto r = floatColorToByte(color[0]);
  auto g = floatColorToByte(color[1]);
  auto b = floatColorToByte(color[2]);
  auto a = floatColorToByte(color[3]);

  return {
      {x0, y0, depth, x0 - minX, y0 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
      {x1, y1, depth, x1 - minX, y1 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
      {x2, y2, depth, x2 - minX, y2 - minY, w, h, r, g, b, a, 0.0f, 0, 0, 0, 0, 0.0f},
  };
}

namespace {

// Draw one port glyph from the PortGlyph the forEachPort walk produced: a
// relationship pin draws an authoring triangle on the node's live right edge
// (outer ring + inset fill); every other port draws a filled circle quad at the
// glyph's live center with a per-vertex ring color and a strokeWidth in the
// `innerStroke` slot (the `selected = -1` sentinel tells the node shader to use
// the per-vertex stroke color). Non-relationship glyphs are drawn only when connected
// or being dragged from (unconnected pins stay hidden until linked), matching the
// Python visibility gate. The center is already live (forEachPort applied the
// node's drag shift), so no post-shift is needed.
void addPortGlyph(
    PortHighlightVertices& out,
    const PortGlyph& glyph,
    double portRadius,
    const RenderConfig& config,
    const PortHighlightState& state) {
  auto depth = static_cast<float>(RenderConfig::contentDepth(glyph.node.zOrder));
  bool isHovered = state.hasHover && state.hoveredNodeId == glyph.nodeId &&
      state.hoveredPort == glyph.pinName && state.hoveredIsOutput == glyph.isOutput;

  if (!glyph.isRelationship) {
    bool isDragSource = state.draggingLink && state.dragSourceNodeId == glyph.nodeId &&
        state.dragSourcePort == glyph.pinName && state.dragSourceIsOutput == glyph.isOutput;
    if (!(glyph.isConnected || isDragSource)) {
      return;
    }
  }

  double ringThickness = config.nodePortRingThickness;
  std::array<float, 4> fillColor =
      glyph.isConnected ? config.portConnectedFillColor : config.portDisconnectedFillColor;
  std::array<float, 4> ringColor =
      glyph.isConnected ? config.portConnectedRingColor : config.portDisconnectedRingColor;
  if (isHovered) {
    ringColor = config.portHoverColor;
    ringThickness *= 2.0;
  }

  if (glyph.isRelationship) {
    // USD relationships are outgoing references, so the glyph always sits on the
    // node's live right (output) edge even when the row was classified as input.
    double portX = glyph.node.position[0] + glyph.node.size[0] + glyph.shiftX;
    double portCenterY = glyph.centerY;
    double tipX = portX + portRadius;
    double baseX = portX - portRadius;
    double topY = portCenterY - portRadius;
    double botY = portCenterY + portRadius;
    auto outer = NodeRenderManager::buildTriangleEndpointVertices(
        static_cast<float>(tipX),
        static_cast<float>(portCenterY),
        static_cast<float>(baseX),
        static_cast<float>(topY),
        static_cast<float>(baseX),
        static_cast<float>(botY),
        depth,
        ringColor);
    double insetRatio =
        std::min(0.6, std::max(0.18, ringThickness / std::max(portRadius * 2.0, 1.0)));
    double cx = (tipX + baseX + baseX) / 3.0;
    double cy = (portCenterY + topY + botY) / 3.0;
    auto inset = [&](double x, double y) {
      return std::pair<double, double>(x + (cx - x) * insetRatio, y + (cy - y) * insetRatio);
    };
    auto [ix0, iy0] = inset(tipX, portCenterY);
    auto [ix1, iy1] = inset(baseX, topY);
    auto [ix2, iy2] = inset(baseX, botY);
    auto inner = NodeRenderManager::buildTriangleEndpointVertices(
        static_cast<float>(ix0),
        static_cast<float>(iy0),
        static_cast<float>(ix1),
        static_cast<float>(iy1),
        static_cast<float>(ix2),
        static_cast<float>(iy2),
        depth,
        fillColor);
    out.relationship.insert(out.relationship.end(), outer.begin(), outer.end());
    out.relationship.insert(out.relationship.end(), inner.begin(), inner.end());
    return;
  }

  // The connected/disconnected port circle is the same rounded-quad the endpoint
  // helper builds: fill in the color slot, ring color + thickness in the stroke
  // slots, nodeIndex -1. The center is already live (forEachPort applied the
  // node's drag shift), so no post-shift is needed. Reuse
  // buildCircleEndpointVertices so the port-circle vertex layout lives in one place.
  auto circle = NodeRenderManager::buildCircleEndpointVertices(
      static_cast<float>(glyph.centerX),
      static_cast<float>(glyph.centerY),
      static_cast<float>(portRadius),
      depth,
      fillColor,
      ringColor,
      static_cast<float>(ringThickness));
  out.regular.insert(out.regular.end(), circle.begin(), circle.end());
}

// Enumerate one node's logical ports — normal, folded-header aggregate, dual /
// output-dual mirror, synthetic relationship, title-collapsed aggregate — passing
// each as a live (drag-shifted), side-correct PortGlyph to `visit`. The single
// per-node geometry walk shared by the highlight producer (over every node) and
// the hit-test (over spatial candidates).
void forEachPortOnNode(
    const GraphModel& graph,
    const std::string& nodeId,
    const NodeData& node,
    const NodeTransformFrame* transformFrame,
    const std::function<void(const PortGlyph&)>& visit) {
  // A node's TRUE on-screen position is its immutable base + the live move offset
  // — the sum the node-quad / text shaders compute from the transform texture
  // (syncFromNodes never moves an existing node's base; transforms_ accumulates
  // every drag). The snapshot (node.position) is re-synced to the live position
  // while the base stays put, so shifting by the offset alone would double-count.
  // Shift by (base + offset) - snapshot, which lands exactly on the quads/text in
  // every state — it equals the drag delta mid-drag and 0 once the snapshot
  // catches up (and 0 with no frame / unknown node). Ports are emitted at their
  // live center so the drawn circle and its hit region share one position.
  double shiftX = 0.0;
  double shiftY = 0.0;
  if (transformFrame != nullptr && transformFrame->shaderIndex(nodeId) >= 0.0f) {
    Vec2d base = transformFrame->basePosition(nodeId);
    Vec2d nodeOffset = transformFrame->offset(nodeId);
    shiftX = (base[0] - node.position[0]) + nodeOffset[0];
    shiftY = (base[1] - node.position[1]) + nodeOffset[1];
  }
  double leftX = node.position[0] + shiftX;
  double rightX = node.position[0] + node.size[0] + shiftX;

  auto emit = [&](const std::string& pinName,
                  bool isOutput,
                  double centerX,
                  double centerY,
                  bool isConnected,
                  bool isRelationship) {
    visit(
        PortGlyph{
            nodeId,
            node,
            pinName,
            isOutput,
            centerX,
            centerY,
            shiftX,
            shiftY,
            isConnected,
            isRelationship});
  };

  // Title-collapsed: one aggregate port per side, connected if ANY original
  // (pre-grouping) pin on that side carries a link. No per-pin / synthetic pass.
  if (node.titleCollapsed) {
    double aggregateY = node.position[1] + node.size[1] * 0.5 + shiftY;
    const std::vector<std::string>& origIn =
        node.originalInputPins.empty() ? node.inputPins : node.originalInputPins;
    const std::vector<std::string>& origOut =
        node.originalOutputPins.empty() ? node.outputPins : node.originalOutputPins;
    if (!origIn.empty()) {
      bool hasInput = false;
      for (const auto& pin : origIn) {
        if (graph.isPortConnected(nodeId, pin, false)) {
          hasInput = true;
          break;
        }
      }
      emit("__aggregate_input__", false, leftX, aggregateY, hasInput, false);
    }
    if (!origOut.empty()) {
      bool hasOutput = false;
      for (const auto& pin : origOut) {
        if (graph.isPortConnected(nodeId, pin, true)) {
          hasOutput = true;
          break;
        }
      }
      emit("__aggregate_output__", true, rightX, aggregateY, hasOutput, false);
    }
    return;
  }

  // Input pins.
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    int kind = (i < static_cast<int>(node.inputRowKinds.size())) ? node.inputRowKinds[i] : 0;
    if (kind == 2) { // unfolded header: no port circle
      continue;
    }
    const std::string& pinName = node.inputPins[i];
    double centerY = node.position[1] +
        ((i < static_cast<int>(node.layoutInputCenterY.size())) ? node.layoutInputCenterY[i]
                                                                : node.size[1] * 0.5) +
        shiftY;
    bool isConnected = (kind == 1) ? graph.isFoldedHeaderConnected(nodeId, pinName, false)
                                   : graph.isPortConnected(nodeId, pinName, false);
    bool isRel = node.relationshipInputPins.count(pinName) > 0;
    emit(pinName, false, leftX, centerY, isConnected, isRel);

    // Dual pin: also expose the mirrored output port on the right edge.
    if (!isRel && node.dualPinNames.count(pinName) > 0) {
      emit(pinName, true, rightX, centerY, graph.isPortConnected(nodeId, pinName, true), false);
    }
  }

  // Output pins.
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    int kind = (i < static_cast<int>(node.outputRowKinds.size())) ? node.outputRowKinds[i] : 0;
    if (kind == 2) {
      continue;
    }
    const std::string& pinName = node.outputPins[i];
    double centerY = node.position[1] +
        ((i < static_cast<int>(node.layoutOutputCenterY.size())) ? node.layoutOutputCenterY[i]
                                                                 : node.size[1] * 0.5) +
        shiftY;
    bool isConnected = (kind == 1) ? graph.isFoldedHeaderConnected(nodeId, pinName, true)
                                   : graph.isPortConnected(nodeId, pinName, true);
    bool isRel = node.relationshipOutputPins.count(pinName) > 0;
    emit(pinName, true, rightX, centerY, isConnected, isRel);

    // Output-dual pin: also expose the mirrored input port on the left edge.
    if (!isRel && node.outputDualPinNames.count(pinName) > 0) {
      emit(pinName, false, leftX, centerY, graph.isPortConnected(nodeId, pinName, false), false);
    }
  }

  // Synthetic relationship ports: relationship endpoints with no visible row,
  // surfaced as aggregate handles near the title (always "connected").
  double titleHeight =
      node.layoutTitleHeight > 0.0 ? node.layoutTitleHeight : std::min(node.size[1], 32.0);
  double synthY = node.position[1] + titleHeight * 0.5 + shiftY;
  for (const auto& [pinName, isOutput] : graph.syntheticRelationshipPortsForNode(nodeId)) {
    double portX = isOutput ? rightX : leftX;
    emit(pinName, isOutput, portX, synthY, /*isConnected=*/true, /*isRelationship=*/true);
  }
}

} // namespace

void NodeRenderManager::forEachPort(
    const GraphModel& graph,
    const NodeTransformFrame* transformFrame,
    const std::function<void(const PortGlyph&)>& visit) {
  for (const auto& [nodeId, node] : graph.nodes) {
    forEachPortOnNode(graph, nodeId, node, transformFrame, visit);
  }
}

PortHighlightVertices NodeRenderManager::buildPortHighlightVertices(
    const GraphModel& graph,
    const RenderConfig& config,
    const PortHighlightState& state,
    const NodeTransformFrame* transformFrame) {
  PortHighlightVertices out;
  double portRadius = config.nodePortWidth * 0.5; // mirrors GraphNodeRenderer::getPortWidth()
  // Reserve up to 6 verts (one circle = two triangles) per displayed port across
  // all nodes so the per-glyph emplace_backs don't repeatedly reallocate on large
  // graphs. This is an upper bound (unconnected pins are skipped); relationship
  // triangles are rarer and left to grow on demand.
  size_t portCountEstimate = 0;
  for (const auto& nodeEntry : graph.nodes) {
    const NodeData& node = nodeEntry.second;
    portCountEstimate += node.inputPins.size() + node.outputPins.size();
  }
  out.regular.reserve(portCountEstimate * 6);
  forEachPort(graph, transformFrame, [&](const PortGlyph& glyph) {
    addPortGlyph(out, glyph, portRadius, config, state);
  });
  return out;
}

NodeRenderManager::PortHit NodeRenderManager::portAtPoint(
    const GraphModel& graph,
    const Vec2d& worldPos,
    double hitRadius,
    const NodeTransformFrame* transformFrame) {
  double hitRadiusSq = hitRadius * hitRadius;
  PortHit closest;
  double closestDistSq = 1e30;

  // Live per-node shift from an in-progress drag (NodeTransformFrame), so the hit
  // region tracks the dragged position.
  auto nodeShift = [&](const std::string& nid, const NodeData& node) -> Vec2d {
    if (transformFrame != nullptr && transformFrame->shaderIndex(nid) >= 0.0f) {
      const Vec2d base = transformFrame->basePosition(nid);
      const Vec2d nodeOffset = transformFrame->offset(nid);
      return {
          (base[0] - node.position[0]) + nodeOffset[0],
          (base[1] - node.position[1]) + nodeOffset[1]};
    }
    return {0.0, 0.0};
  };

  // Occlusion: a lower node's ports hidden under a higher node's body must not be
  // pickable. Find the highest zOrder among nodes whose body (the drawn
  // background rect, not expanded by the hit radius) covers the cursor; ports on
  // any node below that are skipped below. This mirrors the z-order that node
  // picking (_getNodeIdAtWorldPos) already respects but portAtPoint did not.
  bool hasTopBody = false;
  int topBodyZ = 0;
  for (const auto& [nodeId, node] : graph.nodes) {
    const Vec2d shift = nodeShift(nodeId, node);
    const double bx0 = node.position[0] + shift[0];
    const double by0 = node.position[1] + shift[1];
    if (worldPos[0] >= bx0 && worldPos[0] <= bx0 + node.size[0] && worldPos[1] >= by0 &&
        worldPos[1] <= by0 + node.size[1]) {
      if (!hasTopBody || node.zOrder > topBodyZ) {
        hasTopBody = true;
        topBodyZ = node.zOrder;
      }
    }
  }

  for (const auto& [nodeId, node] : graph.nodes) {
    // Structured bindings cannot be captured by a lambda before C++20; alias to a
    // plain reference so the hit visitor can capture it.
    const std::string& nid = nodeId;

    // Skip ports occluded by a higher node's body at the cursor (a port on the
    // top node itself is never occluded, since nothing has a higher zOrder there).
    if (hasTopBody && topBodyZ > node.zOrder) {
      continue;
    }

    // Cheap reject: the cursor must fall inside the node's live AABB expanded by
    // the hit radius (every port glyph sits within that band), so the per-port
    // walk only runs for the handful of nodes near the cursor — preserving the
    // spatial-index locality the Python hover path had.
    const Vec2d shift = nodeShift(nid, node);
    double minX = node.position[0] + shift[0] - hitRadius;
    double maxX = node.position[0] + node.size[0] + shift[0] + hitRadius;
    double minY = node.position[1] + shift[1] - hitRadius;
    double maxY = node.position[1] + node.size[1] + shift[1] + hitRadius;
    if (worldPos[0] < minX || worldPos[0] > maxX || worldPos[1] < minY || worldPos[1] > maxY) {
      continue;
    }

    forEachPortOnNode(graph, nodeId, node, transformFrame, [&](const PortGlyph& glyph) {
      double dx = worldPos[0] - glyph.centerX;
      double dy = worldPos[1] - glyph.centerY;
      double distSq = dx * dx + dy * dy;
      // Unlike the highlight producer this ignores the connected/visibility gate:
      // a disconnected pin is still a valid hover / link-drag target.
      if (distSq > hitRadiusSq || distSq >= closestDistSq) {
        return;
      }
      // A title-collapsed node exposes a single aggregate handle per side; map it
      // back to a concrete pin (the first display pin on that side, matching the
      // old per-pin getPortAtPoint) so callers get a real port name.
      std::string pinName = glyph.pinName;
      if (pinName == "__aggregate_input__" || pinName == "__aggregate_output__") {
        const std::vector<std::string>& pins =
            glyph.isOutput ? glyph.node.outputPins : glyph.node.inputPins;
        if (pins.empty()) {
          return;
        }
        pinName = pins.front();
      }
      closestDistSq = distSq;
      closest.found = true;
      closest.nodeId = nid;
      closest.portName = pinName;
      closest.isOutput = glyph.isOutput;
    });
  }

  return closest;
}

uint32_t NodeRenderManager::hashPortHighlightVertices(const std::vector<NodeVertex>& vertices) {
  uint64_t h = 1469598103934665603ull; // FNV-1a offset basis
  const auto mix = [&h](const void* p, size_t n) {
    const auto* bytes = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) {
      h = (h ^ bytes[i]) * 1099511628211ull; // FNV-1a prime
    }
  };
  for (const NodeVertex& vtx : vertices) {
    mix(&vtx.x, sizeof(float) * 7); // x,y,z,u,v,w,h are contiguous floats
    mix(&vtx.r, 4); // r,g,b,a
    mix(&vtx.innerStroke, sizeof(float));
    mix(&vtx.sr, 4); // sr,sg,sb,sa
    mix(&vtx.selected, sizeof(float));
    mix(&vtx.nodeIndex, sizeof(float));
  }
  return static_cast<uint32_t>(h ^ (h >> 32));
}

void NodeRenderManager::renderPortHighlightsFromGraph(
    const GraphModel& graph,
    const RenderConfig& config,
    const PortHighlightState& state,
    const float* projection4x4,
    float cornerRadius,
    bool geometryChanged) {
  if (!initialized_ || !shaders_ || projection4x4 == nullptr) {
    return;
  }
  // Rebuild the highlight geometry only when something it depends on changed:
  // the hover / link-drag state (compared here) or the node geometry /
  // connections / colors / a node drag (signalled by geometryChanged from the
  // caller). buildPortHighlightVertices walks every port and bakes each node's
  // drag offset per-vertex, and hashPortHighlightVertices walks the result, so
  // doing both unconditionally was O(ports) of wasted CPU work on idle / pan /
  // zoom frames. The shared transform frame supplies each node's live move
  // offset so port glyphs follow a drag.
  if (geometryChanged || !havePortHighlightCache_ || state != lastPortHighlightState_) {
    cachedPortHighlightVerts_ = buildPortHighlightVertices(graph, config, state, transformFrame_);
    cachedPortHighlightRegularHash_ = hashPortHighlightVertices(cachedPortHighlightVerts_.regular);
    cachedPortHighlightRelationshipHash_ =
        hashPortHighlightVertices(cachedPortHighlightVerts_.relationship);
    lastPortHighlightState_ = state;
    havePortHighlightCache_ = true;
  }

  static const std::array<float, 4> kNoInnerStroke{0.0f, 0.0f, 0.0f, 0.0f};
  // Re-upload each highlight VBO only when its geometry actually changed: the
  // cached hash is the batch "generation" with selectionChanged=false, so
  // createOrUpdateVBO skips the per-frame glBufferSubData (a Metal command-buffer
  // submission) on idle / pan / zoom frames. The draw still issues every frame
  // from the persistent VBO. Distinct batch keys keep the circle and triangle
  // VBOs separate. Relationship triangles draw with corner radius 0 (they are
  // not rounded quads), matching the Python _drawNodeVertices call.
  if (!cachedPortHighlightVerts_.regular.empty()) {
    renderNodes(
        cachedPortHighlightVerts_.regular,
        0x000000FFu,
        cachedPortHighlightRegularHash_,
        false,
        projection4x4,
        cornerRadius,
        kNoInnerStroke);
  }
  if (!cachedPortHighlightVerts_.relationship.empty()) {
    renderNodes(
        cachedPortHighlightVerts_.relationship,
        0x000000FEu,
        cachedPortHighlightRelationshipHash_,
        false,
        projection4x4,
        0.0f,
        kNoInnerStroke);
  }
}

void NodeRenderManager::renderEndpointCircle(
    float cx,
    float cy,
    float radius,
    float depth,
    const float* projection4x4,
    const std::array<float, 4>& fillColor,
    const std::array<float, 4>& strokeColor,
    float strokeWidth) {
  if (!initialized_ || !shaders_ || radius <= 0.0f) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices =
      buildCircleEndpointVertices(cx, cy, radius, depth, fillColor, strokeColor, strokeWidth);
  drawVerticesImmediate(vertices, shader, projection4x4, radius, {0.0f, 0.0f, 0.0f, 0.0f});
}

void NodeRenderManager::renderEndpointTriangle(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float depth,
    const float* projection4x4,
    const std::array<float, 4>& color) {
  if (!initialized_ || !shaders_) {
    return;
  }

  auto* shader = shaders_->get("node");
  if (!shader) {
    return;
  }

  auto vertices = buildTriangleEndpointVertices(x0, y0, x1, y1, x2, y2, depth, color);
  drawVerticesImmediate(vertices, shader, projection4x4, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f});
}

void NodeRenderManager::createOrUpdateVBO(
    RenderBatch& batch,
    const std::vector<NodeVertex>& vertices,
    uint32_t generation,
    bool selectionChanged) {
  int vertexCount = static_cast<int>(vertices.size());

  // Fast path for idle / pan / zoom / move frames: if the batch already exists
  // and neither its generation, vertex count, nor selection changed, the GPU
  // buffer is already current and there is nothing to upload. Bail BEFORE
  // packing -- NodeVertex::packBatch is O(vertices) CPU work, and packing the
  // whole buffer only to discard it at the unchanged check below was the
  // dominant per-frame cost on large graphs during a pan (the node-quad and
  // port-highlight VBOs both route through here).
  if (batch.vbo != 0 && batch.cachedGeneration == generation &&
      batch.cachedVertexCount == vertexCount && !selectionChanged) {
    return;
  }

  auto packedData = NodeVertex::packBatch(vertices);
  auto dataSize = static_cast<GLsizeiptr>(packedData.size());

  if (batch.vbo == 0) {
    setupVAO(batch, dataSize, packedData.data());
    batch.cachedGeneration = generation;
    batch.cachedVertexCount = vertexCount;
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
  if (batch.cachedVertexCount == vertexCount) {
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, packedData.data());
  } else {
    glBufferData(GL_ARRAY_BUFFER, dataSize, packedData.data(), GL_DYNAMIC_DRAW);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  batch.cachedGeneration = generation;
  batch.cachedVertexCount = vertexCount;
}

void NodeRenderManager::setupVAO(RenderBatch& batch, GLsizeiptr dataSize, const void* data) {
  glGenBuffers(1, &batch.vbo);
  glGenVertexArrays(1, &batch.vao);
  if (batch.vbo == 0 || batch.vao == 0) {
    // GL context unusable.  Bail rather than issue draws against the default
    // object — that's GL_INVALID_OPERATION in core profile and crashes the
    // NVIDIA driver on its worker thread.  renderNodes will retry next frame.
    if (batch.vbo != 0) {
      glDeleteBuffers(1, &batch.vbo);
      batch.vbo = 0;
    }
    if (batch.vao != 0) {
      glDeleteVertexArrays(1, &batch.vao);
      batch.vao = 0;
    }
    return;
  }

  glBindVertexArray(batch.vao);
  glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
  glBufferData(GL_ARRAY_BUFFER, dataSize, data, GL_DYNAMIC_DRAW);

  NodeVertex::setupVertexAttribs();

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void NodeRenderManager::invalidateAll() {
  vertexCache_.invalidateAll();
  nodeQuadNeedsRebuild_ = true;
}

void NodeRenderManager::invalidateNode(const std::string& nodeId) {
  vertexCache_.invalidateNode(nodeId);
  // No per-node node-quad cache yet: any node change forces a full rebuild.
  nodeQuadNeedsRebuild_ = true;
}

void NodeRenderManager::clearCache() {
  vertexCache_.clear();
  for (auto& [color, batch] : batches_) {
    if (batch.vbo) {
      glDeleteBuffers(1, &batch.vbo);
    }
    if (batch.vao) {
      glDeleteVertexArrays(1, &batch.vao);
    }
  }
  batches_.clear();

  if (nodeQuadBatch_.vbo) {
    glDeleteBuffers(1, &nodeQuadBatch_.vbo);
    nodeQuadBatch_.vbo = 0;
  }
  if (nodeQuadBatch_.vao) {
    glDeleteVertexArrays(1, &nodeQuadBatch_.vao);
    nodeQuadBatch_.vao = 0;
  }
  nodeQuadBatch_.cachedGeneration = 0;
  nodeQuadBatch_.cachedVertexCount = 0;
}

void NodeRenderManager::cleanup() {
  clearCache();
  // transformFrame_ is owned elsewhere (shared); don't delete its texture here.
  nodeQuadVertexData_.clear();
  nodeQuadNeedsRebuild_ = true;
  shaders_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
