// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_ICON_RENDER_MANAGER_H
#define NOODLES_RENDER_ICON_RENDER_MANAGER_H

#include "core/NodeData.h"
#include "core/api.h"
#include "core/pragmas.h"
#include "render/IconVertexCache.h"
#include "render/NodeTransformFrame.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noodles {

struct RenderConfig;

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

/// C++-owned icon-render producer (peer of TextRenderManager). This diff adds
/// only the pure geometry producer: it turns a node's content + cached layout
/// metrics into the icon quads the GPU draws — title icon + one per visible pin
/// row (minus, or the relationship double-arrow). The cache, viewport cull, GL
/// draw, and bindings land in later diffs; nothing calls this yet.
///
/// Mirrors the Python iconRenderer placement so the cutover is pixel-faithful:
/// the title icon sits after the title caret + node icon, and each row icon is
/// left-aligned at the row's vertical centre.
class NOODLES_API IconRenderManager {
 public:
  // Logical texture keys for the two fixed row icons and the default title icon.
  // The GL path (later diff) resolves these to assets/icons/<key>.png; a node's
  // own titleIconPath is used verbatim as the key when it is non-empty.
  static constexpr const char* kRowMinusIconKey = "row-minus";
  static constexpr const char* kRelationshipRowIconKey = "relationship-arrows";
  static constexpr const char* kDefaultTitleIconKey = "box";

  // One icon to draw, in world space at the node's base frame (x = left edge,
  // y = vertical centre; the quad is `size` x `size`). textureId is a logical
  // key (above) or a node's titleIconPath.
  struct IconQuad {
    double x = 0.0;
    double y = 0.0;
    double size = 0.0;
    float depth = 0.0f;
    std::string textureId;
  };

  // Pure producer: a node's icon quads, laid out at (baseX, baseY) — the node's
  // transform-frame base, so they ride a drag via the transform texture exactly
  // like the node quad and text. defaultTitleIconKey supplies the title texture
  // when the node has no titleIconPath. The title icon always shows (even when
  // collapsed); row icons are emitted once per visible row slot, skipping fold
  // carets (rowKind 1/2), and use the relationship double-arrow when the row's
  // pin is a relationship pin on either side.
  static std::vector<IconQuad> buildNodeIconQuads(
      const NodeData& node,
      const RenderConfig& config,
      double baseX,
      double baseY,
      const std::string& defaultTitleIconKey = kDefaultTitleIconKey);

  // Append a quad's 6 vertices (two triangles) to `out`, each as
  // (x, y, z, u, v, nodeIndex) to match icon_vert.glsl. UVs run (0,0) top-left
  // to (1,1) bottom-right; the GL path loads textures to match.
  static void
  appendIconQuadVertices(const IconQuad& quad, float nodeIndex, std::vector<float>& out);

  // Per-node input to icon-buffer assembly (mirrors NodeTextAssemblyItem).
  struct NodeIconAssemblyItem {
    const std::vector<float>* vertices = nullptr; // IconVertexCache::getVertices(nodeId)
    const std::vector<std::string>* textureIds =
        nullptr; // getTextureIds(nodeId): one per quad (6 verts)
    float depth = 0.0f;
    float nodeIndex = 0.0f;
    std::string nodeId;
  };

  // One drawable run (one icon quad = 6 vertices) tagged with its texture + owning node.
  struct IconDrawItem {
    std::string textureId;
    int firstVertex = 0; // vertex index into the assembled buffer
    int vertexCount = 0; // 6
    std::string nodeId; // for viewport culling
  };

  // Concatenate per-node icon vertex blobs into one buffer, re-baking each item's
  // depth (float offset 2) and nodeIndex (float offset 5) over its vertices — so a
  // z-order or transform-slot change does not require re-layout — and emit one
  // IconDrawItem per quad (textureId from the item's textureIds, parallel to quad
  // order). Skips null/empty blobs. Pure (no GL). Mirrors assembleNodeTextBuffer.
  static std::vector<float> assembleIconBuffer(
      const std::vector<NodeIconAssemblyItem>& items,
      std::vector<IconDrawItem>& outDrawItems);

  // Viewport cull: keep only draw items whose node is on-screen, tested at the SAME
  // world position the GPU draws the icons at — the transform-frame base + live move
  // offset — never the GraphModel snapshot (which goes stale on a drag). For a node
  // with transformFrame->shaderIndex(nodeId) >= 0.0f use basePosition + offset; else
  // fall back to node.position. The node AABB is (origin, origin + node.size); an
  // item whose node box is fully outside [vpMin, vpMax] is dropped. The surviving
  // items keep their original order so the caller can group them by textureId.
  static std::vector<IconDrawItem> computeVisibleIconDrawItems(
      const std::vector<IconDrawItem>& drawItems,
      const std::unordered_map<std::string, NodeData>& nodes,
      const NodeTransformFrame* transformFrame,
      double vpMinX,
      double vpMinY,
      double vpMaxX,
      double vpMaxY);

  IconRenderManager() = default;
  ~IconRenderManager();

  IconRenderManager(const IconRenderManager&) = delete;
  IconRenderManager& operator=(const IconRenderManager&) = delete;
  IconRenderManager(IconRenderManager&&) = delete;
  IconRenderManager& operator=(IconRenderManager&&) = delete;

  // Store the shader library + assets root (used to resolve logical icon keys to
  // assets/icons/<key>.png) and mark the manager ready to draw.
  void initialize(ShaderLibrary* shaders, const std::string& assetsPath);

  // The shared node-local coordinate frame (base positions + move offsets +
  // transform texture). Non-owning; the same frame is shared with the text /
  // node-quad paths so icons move together with them on a drag.
  void setTransformFrame(NodeTransformFrame* frame) {
    transformFrame_ = frame;
  }

  // Force a full icon re-layout + buffer rebuild on the next render.
  void markIconsDirty() {
    iconNeedsRebuild_ = true;
  }

  // Whether the next render will rebuild the icon buffer (peer of
  // TextRenderManager::needsTextRebuild). renderIconsFromGraph rebuilds on
  // contentChanged OR markIconsDirty, so the Python caller re-syncs
  // GraphModel.nodes whenever this is set — a markIconsDirty()-only frame (one
  // that did not set contentChanged) then rebuilds from fresh node data instead
  // of a stale snapshot.
  bool needsIconRebuild() const {
    return iconNeedsRebuild_;
  }

  // Drop the per-node icon vertex cache and force a rebuild. Icon geometry is
  // baked at each node's transform-frame base position, so any path that
  // re-baselines those bases (NodeTransformFrame::reset on (re)load / add) must
  // clear it or icons would redraw at the stale base. Peer of
  // TextRenderManager::resetPositionCaches. (markIconsDirty alone is not enough:
  // the per-node isDirty signature excludes position, so cached blobs survive.)
  void resetPositionCaches() {
    iconVertexCache_.clear();
    iconNeedsRebuild_ = true;
  }

  // GL-free: (re)build the assembled icon vertex buffer + per-quad draw items
  // from the authoritative node snapshot, reusing per-node cached blobs whose
  // layout signature is unchanged. Lays geometry out at each node's base frame
  // (icons ride the live move offset via the transform texture).
  void generateNodeIconVertices(
      const std::unordered_map<std::string, NodeData>& nodes,
      const RenderConfig& config);

  // Full icon orchestration + GL draw (peer of renderNodeTextFull): rebuilds the
  // buffer on contentChanged / markIconsDirty, uploads the shared transform
  // frame, sets up the "icon" shader, and draws the transform-aware visible draw
  // items batched per texture. Returns true if it drew.
  bool renderIconsFromGraph(
      const std::unordered_map<std::string, NodeData>& nodes,
      const float* projection4x4,
      float zoom,
      bool contentChanged,
      const RenderConfig& config,
      double panX = 0.0,
      double panY = 0.0,
      double viewportWidth = 0.0,
      double viewportHeight = 0.0,
      bool cullOffscreen = false);

  // Delete GL handles (VBO/VAO + cached textures) and clear state. Does NOT
  // delete the shared, non-owning transform frame.
  void cleanup();

  // Test-only.
  const IconVertexCache& iconVertexCacheForTest() const {
    return iconVertexCache_;
  }

 private:
  static void collectRowIconSlots(
      const std::vector<std::string>& pins,
      const std::vector<int>& rowKinds,
      const std::vector<int>& rowSlots,
      const std::unordered_set<std::string>& relationshipPins,
      std::map<int, bool>& outSlotIsRelationship);

  // Return a cached GL texture for an icon id. A logical key resolves to
  // assets/icons/<key>.png; any other id is treated as a filesystem path (a
  // node's titleIconPath). A failed load falls back to the default box texture;
  // the result is cached by id so a miss is not retried every frame.
  GLuint resolveTexture(const std::string& textureId);

  IconVertexCache iconVertexCache_;
  NodeTransformFrame* transformFrame_ = nullptr; // shared, non-owning
  ShaderLibrary* shaders_ = nullptr;
  std::string assetsPath_;
  bool initialized_ = false;

  // textureId -> GL texture name. A logical key or a node's titleIconPath maps to
  // one texture; a failed load maps to the box fallback so it is not retried.
  std::unordered_map<std::string, GLuint> textureCache_;

  // Assembled icon vertex buffer + the per-quad draw items (parallel to the
  // buffer) that the GL draw groups by texture and culls by node.
  std::vector<float> iconVertexData_;
  std::vector<IconDrawItem> iconDrawItems_;

  GLuint iconVbo_ = 0;
  GLuint iconVao_ = 0;
  bool iconNeedsRebuild_ = true;
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_ICON_RENDER_MANAGER_H
