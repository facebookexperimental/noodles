// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/IconRenderManager.h"

#include "core/RenderConfig.h"
#include "render/NodeTransformFrame.h"

#include "stb_image.h"

#include <array>
#include <unordered_set>

namespace noodles {

namespace {

// Vertex stride matches icon_vert.glsl / appendIconQuadVertices: each vertex is
// (x, y, z, u, v, nodeIndex); 6 vertices per quad. depth lives at float offset 2,
// nodeIndex at float offset 5 (shared with the text path).
constexpr int kFloatsPerVertex = 6;
constexpr int kVertsPerQuad = 6;
constexpr int kFloatsPerQuad = kFloatsPerVertex * kVertsPerQuad; // 36
constexpr int kDepthOffset = 2;
constexpr int kNodeIndexOffset = 5;

// Load an RGBA texture from disk into a new GL texture, mirroring FontAtlas's
// stb_image recipe. Returns 0 on failure (missing file / GL function pointers
// unresolved on a partial OSMesa loader).
GLuint loadTextureFromFile(const std::string& path) {
  int width = 0, height = 0, channels = 0;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
  if (!data) {
    return 0;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(data);
  return texture;
}

// True when textureId is one of the fixed logical keys (so it resolves to
// assets/icons/<key>.png rather than being treated as a node titleIconPath).
bool isLogicalIconKey(const std::string& textureId) {
  return textureId == IconRenderManager::kRowMinusIconKey ||
      textureId == IconRenderManager::kRelationshipRowIconKey ||
      textureId == IconRenderManager::kDefaultTitleIconKey;
}

} // anonymous namespace

void IconRenderManager::collectRowIconSlots(
    const std::vector<std::string>& pins,
    const std::vector<int>& rowKinds,
    const std::vector<int>& rowSlots,
    const std::unordered_set<std::string>& relationshipPins,
    std::map<int, bool>& outSlotIsRelationship) {
  for (size_t i = 0; i < pins.size(); ++i) {
    const int kind = (i < rowKinds.size()) ? rowKinds[i] : 0;
    // rowKind 1/2 are fold carets (collapsed / expanded group headers); they get
    // a caret glyph, not a minus/relationship icon.
    if (kind == 1 || kind == 2) {
      continue;
    }
    const int slot = (i < rowSlots.size()) ? rowSlots[i] : static_cast<int>(i);
    if (slot < 0) {
      continue; // hidden row (e.g. a folded child)
    }
    const bool isRelationship = relationshipPins.count(pins[i]) > 0;
    auto it = outSlotIsRelationship.find(slot);
    if (it == outSlotIsRelationship.end()) {
      outSlotIsRelationship[slot] = isRelationship;
    } else {
      // A row shared by an input and an output pin (a dual row) draws one icon;
      // it is a relationship row if EITHER side is a relationship pin.
      it->second = it->second || isRelationship;
    }
  }
}

std::vector<IconRenderManager::IconQuad> IconRenderManager::buildNodeIconQuads(
    const NodeData& node,
    const RenderConfig& config,
    double baseX,
    double baseY,
    const std::string& defaultTitleIconKey) {
  std::vector<IconQuad> quads;

  const double titleFontSize = config.get("nodeTitleFontSize", 24.0);
  const double marginH = config.get("nodeMarginH", 16.0);
  const auto depth = static_cast<float>(RenderConfig::contentDepth(node.zOrder));

  // Title icon: after the title caret + node-icon gap, vertically centred in the
  // title bar. Always shown, even when the node is collapsed.
  {
    const double caretSize = titleFontSize * 0.5;
    const double caretGap = titleFontSize * 0.15;
    IconQuad title;
    title.x = baseX + marginH + caretSize + caretGap;
    title.y = baseY + node.layoutTitleHeight * 0.5;
    title.size = titleFontSize * 1.2;
    title.depth = depth;
    title.textureId = node.titleIconPath.empty() ? defaultTitleIconKey : node.titleIconPath;
    quads.push_back(std::move(title));
  }

  if (node.titleCollapsed) {
    return quads; // collapsed: rows are hidden, only the title icon remains
  }

  // Row icons: one per visible row slot, deduped across the input and output
  // sides (ordered by slot for deterministic output).
  std::map<int, bool> slotIsRelationship;
  collectRowIconSlots(
      node.inputPins,
      node.inputRowKinds,
      node.inputRowSlots,
      node.relationshipInputPins,
      slotIsRelationship);
  collectRowIconSlots(
      node.outputPins,
      node.outputRowKinds,
      node.outputRowSlots,
      node.relationshipOutputPins,
      slotIsRelationship);

  const double portStartY = node.layoutPortStartY;
  const double portLineHeight = node.layoutPortLineHeight;
  for (const auto& [slot, isRelationship] : slotIsRelationship) {
    const double ratio = isRelationship ? RenderConfig::kRelationshipRowIconSizeRatio
                                        : RenderConfig::kRowIconSizeRatio;
    IconQuad row;
    row.x = baseX + marginH;
    row.y = baseY + portStartY + slot * portLineHeight + portLineHeight * 0.5;
    row.size = portLineHeight * ratio;
    row.depth = depth;
    row.textureId = isRelationship ? kRelationshipRowIconKey : kRowMinusIconKey;
    quads.push_back(std::move(row));
  }

  return quads;
}

void IconRenderManager::appendIconQuadVertices(
    const IconQuad& quad,
    float nodeIndex,
    std::vector<float>& out) {
  const auto left = static_cast<float>(quad.x);
  const auto right = static_cast<float>(quad.x + quad.size);
  const auto half = static_cast<float>(quad.size * 0.5);
  const auto cy = static_cast<float>(quad.y);
  const float top = cy - half;
  const float bottom = cy + half;
  const float z = quad.depth;

  // Two triangles, (x, y, z, u, v, nodeIndex) per vertex. UVs run (0,0)
  // top-left to (1,1) bottom-right.
  out.insert(out.end(), {left, top, z, 0.0f, 0.0f, nodeIndex});
  out.insert(out.end(), {right, top, z, 1.0f, 0.0f, nodeIndex});
  out.insert(out.end(), {left, bottom, z, 0.0f, 1.0f, nodeIndex});
  out.insert(out.end(), {right, top, z, 1.0f, 0.0f, nodeIndex});
  out.insert(out.end(), {right, bottom, z, 1.0f, 1.0f, nodeIndex});
  out.insert(out.end(), {left, bottom, z, 0.0f, 1.0f, nodeIndex});
}

std::vector<float> IconRenderManager::assembleIconBuffer(
    const std::vector<NodeIconAssemblyItem>& items,
    std::vector<IconDrawItem>& outDrawItems) {
  outDrawItems.clear();

  size_t total = 0;
  for (const auto& item : items) {
    if (item.vertices != nullptr) {
      total += item.vertices->size();
    }
  }

  std::vector<float> buffer;
  buffer.reserve(total);
  for (const auto& item : items) {
    if (item.vertices == nullptr || item.vertices->empty()) {
      continue; // skip null / empty blobs
    }
    int start = static_cast<int>(buffer.size());
    int floatCount = static_cast<int>(item.vertices->size());
    buffer.insert(buffer.end(), item.vertices->begin(), item.vertices->end());
    // Stamp current depth/nodeIndex so cached blobs stay correct after z-order or
    // slot changes without needing a full re-layout (mirrors the text path).
    for (int j = start; j + kFloatsPerVertex <= start + floatCount; j += kFloatsPerVertex) {
      buffer[j + kDepthOffset] = item.depth;
      buffer[j + kNodeIndexOffset] = item.nodeIndex;
    }
    // Emit one draw item per quad, tagged with its texture (parallel to quad
    // order). firstVertex is the vertex index at the quad's start in the buffer.
    int quadCount = floatCount / kFloatsPerQuad;
    for (int q = 0; q < quadCount; ++q) {
      int quadFloatStart = start + q * kFloatsPerQuad;
      std::string textureId =
          (item.textureIds != nullptr && q < static_cast<int>(item.textureIds->size()))
          ? (*item.textureIds)[q]
          : std::string();
      outDrawItems.push_back(
          IconDrawItem{
              std::move(textureId), quadFloatStart / kFloatsPerVertex, kVertsPerQuad, item.nodeId});
    }
  }
  return buffer;
}

std::vector<IconRenderManager::IconDrawItem> IconRenderManager::computeVisibleIconDrawItems(
    const std::vector<IconDrawItem>& drawItems,
    const std::unordered_map<std::string, NodeData>& nodes,
    const NodeTransformFrame* transformFrame,
    double vpMinX,
    double vpMinY,
    double vpMaxX,
    double vpMaxY) {
  std::vector<IconDrawItem> visible;
  visible.reserve(drawItems.size());
  for (const auto& item : drawItems) {
    auto it = nodes.find(item.nodeId);
    if (it == nodes.end()) {
      continue; // unknown node: nothing to draw
    }
    const NodeData& node = it->second;
    // Cull at the SAME world position the GPU draws the icons at — the transform
    // frame's base + live move offset — not the GraphModel snapshot, which is not
    // re-synced on a drag and would strand a dragged node's icons.
    // NodeTransformFrame::liveOrigin centralizes that rule (shared with the text
    // cull, the same rule that fixed the text-cull drag bug).
    const Vec2d origin = (transformFrame != nullptr)
        ? transformFrame->liveOrigin(item.nodeId, node.position)
        : node.position;
    double minX = origin[0];
    double minY = origin[1];
    double maxX = minX + node.size[0];
    double maxY = minY + node.size[1];
    if (maxX < vpMinX || minX > vpMaxX || maxY < vpMinY || minY > vpMaxY) {
      continue; // node box fully outside the viewport
    }
    visible.push_back(item);
  }
  return visible;
}

IconRenderManager::~IconRenderManager() {
  cleanup();
}

void IconRenderManager::initialize(ShaderLibrary* shaders, const std::string& assetsPath) {
  shaders_ = shaders;
  assetsPath_ = assetsPath;
  initialized_ = true;
}

GLuint IconRenderManager::resolveTexture(const std::string& textureId) {
  auto cached = textureCache_.find(textureId);
  if (cached != textureCache_.end()) {
    return cached->second; // includes a previously cached miss (mapped to box)
  }

  // A logical key resolves to a shipped asset; any other id is a node's
  // titleIconPath used verbatim as a filesystem path.
  const std::string path =
      isLogicalIconKey(textureId) ? assetsPath_ + "/icons/" + textureId + ".png" : textureId;
  GLuint texture = loadTextureFromFile(path);

  if (texture == 0 && textureId != kDefaultTitleIconKey) {
    // Fall back to the default box icon (loaded once, then cached), so a missing
    // titleIconPath still draws something rather than nothing.
    texture = resolveTexture(kDefaultTitleIconKey);
  }

  // Cache by id (even a fallback / failed result) so the load is not retried
  // every frame.
  textureCache_[textureId] = texture;
  return texture;
}

void IconRenderManager::generateNodeIconVertices(
    const std::unordered_map<std::string, NodeData>& nodes,
    const RenderConfig& config) {
  // The shared frame owns base positions / move offsets / slot assignment.
  transformFrame_->syncFromNodes(nodes);

  // Prune cached icon blobs for deleted nodes.
  std::unordered_set<std::string> liveIds;
  liveIds.reserve(nodes.size());
  for (const auto& entry : nodes) {
    liveIds.insert(entry.first);
  }
  iconVertexCache_.retainNodes(liveIds);

  // Back-to-front so icon depth matches each node's background quad (shared
  // ordering with the node-quad / text paths keeps the layers in lockstep).
  auto sortedNodes =
      sortNodesByZOrder(const_cast<std::unordered_map<std::string, NodeData>&>(nodes));

  for (const auto& [nodeIdPtr, nodePtr] : sortedNodes) {
    const std::string& nodeId = *nodeIdPtr;
    const NodeData& node = *nodePtr;
    IconLayoutSignature signature = makeIconLayoutSignature(node, config);
    if (iconVertexCache_.isDirty(nodeId, signature)) {
      const Vec2d base = transformFrame_->basePosition(nodeId);
      const float shaderNodeIndex = transformFrame_->shaderIndex(nodeId);
      std::vector<IconQuad> quads =
          buildNodeIconQuads(node, config, base[0], base[1], kDefaultTitleIconKey);
      std::vector<float> verts;
      std::vector<std::string> textureIds;
      textureIds.reserve(quads.size());
      for (const auto& quad : quads) {
        appendIconQuadVertices(quad, shaderNodeIndex, verts);
        textureIds.push_back(quad.textureId);
      }
      iconVertexCache_.update(nodeId, std::move(verts), std::move(textureIds), signature);
    }
  }

  std::vector<NodeIconAssemblyItem> items;
  items.reserve(sortedNodes.size());
  for (const auto& [nodeIdPtr, nodePtr] : sortedNodes) {
    const std::string& nodeId = *nodeIdPtr;
    NodeIconAssemblyItem item;
    item.vertices = &iconVertexCache_.getVertices(nodeId);
    item.textureIds = &iconVertexCache_.getTextureIds(nodeId);
    item.depth = static_cast<float>(RenderConfig::contentDepth(nodePtr->zOrder));
    item.nodeIndex = transformFrame_->shaderIndex(nodeId);
    item.nodeId = nodeId;
    items.push_back(std::move(item));
  }

  iconVertexData_ = assembleIconBuffer(items, iconDrawItems_);
}

bool IconRenderManager::renderIconsFromGraph(
    const std::unordered_map<std::string, NodeData>& nodes,
    const float* projection4x4,
    float zoom,
    bool contentChanged,
    const RenderConfig& config,
    double panX,
    double panY,
    double viewportWidth,
    double viewportHeight,
    bool cullOffscreen) {
  if (nodes.empty() || !initialized_ || !shaders_ || !transformFrame_) {
    return false;
  }

  const bool needsFullUpdate = contentChanged || iconNeedsRebuild_;
  if (needsFullUpdate) {
    generateNodeIconVertices(nodes, config);
    iconNeedsRebuild_ = false;
  }

  if (iconVertexData_.empty() || iconDrawItems_.empty()) {
    return false;
  }

  auto* shader = shaders_->get("icon");
  if (!shader) {
    return false;
  }

  transformFrame_->upload();

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }
  GLint texLoc = shader->getUniformLocation("uTexture");
  if (texLoc >= 0) {
    glUniform1i(texLoc, 0); // icon texture on unit 0
  }
  // Shared transform texture goes on unit 1 (same as the text / node paths).
  transformFrame_->bindToShader(shader, /*textureUnit=*/1);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Icons respect z-order but don't write depth (mirrors the Python icon pass
  // and the text pass), so a higher node's icons never punch holes in lower
  // content that draws afterward.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);

  if (needsFullUpdate || iconVbo_ == 0) {
    if (iconVbo_) {
      glDeleteBuffers(1, &iconVbo_);
    }
    if (iconVao_) {
      glDeleteVertexArrays(1, &iconVao_);
    }
    iconVbo_ = 0;
    iconVao_ = 0;

    glGenBuffers(1, &iconVbo_);
    glGenVertexArrays(1, &iconVao_);

    glBindVertexArray(iconVao_);
    glBindBuffer(GL_ARRAY_BUFFER, iconVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        iconVertexData_.size() * sizeof(float),
        iconVertexData_.data(),
        GL_DYNAMIC_DRAW);

    constexpr int stride = kFloatsPerVertex * 4;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Off-screen culling: keep only the visible nodes' draw items out of the
  // (otherwise unchanged) buffer, mirroring _worldSpaceProjectionMatrix's ortho.
  // When culling is off, draw the held items directly (no per-frame copy).
  std::vector<IconDrawItem> culled;
  const std::vector<IconDrawItem>* drawItems = &iconDrawItems_;
  if (cullOffscreen && zoom > 0.0f && viewportWidth > 0.0 && viewportHeight > 0.0) {
    const double vpMinX = panX;
    const double vpMinY = panY;
    const double vpMaxX = panX + viewportWidth / static_cast<double>(zoom);
    const double vpMaxY = panY + viewportHeight / static_cast<double>(zoom);
    culled = computeVisibleIconDrawItems(
        iconDrawItems_, nodes, transformFrame_, vpMinX, vpMinY, vpMaxX, vpMaxY);
    drawItems = &culled;
  }

  // Group the (order-preserving) draw items by texture so each texture is bound
  // once and its quads drawn in a single glMultiDrawArrays.
  glBindVertexArray(iconVao_);
  std::unordered_map<std::string, std::vector<GLint>> firstsByTexture;
  std::unordered_map<std::string, std::vector<GLsizei>> countsByTexture;
  for (const auto& item : *drawItems) {
    firstsByTexture[item.textureId].push_back(static_cast<GLint>(item.firstVertex));
    countsByTexture[item.textureId].push_back(static_cast<GLsizei>(item.vertexCount));
  }
  bool drew = false;
  for (auto& [textureId, firsts] : firstsByTexture) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveTexture(textureId));
    const std::vector<GLsizei>& counts = countsByTexture[textureId];
    glMultiDrawArrays(
        GL_TRIANGLES, firsts.data(), counts.data(), static_cast<GLsizei>(firsts.size()));
    drew = true;
  }
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  glBindTexture(GL_TEXTURE_2D, 0);
  shader->release();

  return drew;
}

void IconRenderManager::cleanup() {
  if (iconVbo_) {
    glDeleteBuffers(1, &iconVbo_);
    iconVbo_ = 0;
  }
  if (iconVao_) {
    glDeleteVertexArrays(1, &iconVao_);
    iconVao_ = 0;
  }
  // Several ids can map to the same GL texture (a failed titleIconPath aliases
  // the box fallback), so dedupe before deleting to avoid a double-free.
  std::unordered_set<GLuint> deleted;
  for (const auto& [id, texture] : textureCache_) {
    if (texture != 0 && deleted.insert(texture).second) {
      glDeleteTextures(1, &texture);
    }
  }
  textureCache_.clear();

  // transformFrame_ is owned elsewhere (shared); don't delete it here.

  iconVertexData_.clear();
  iconDrawItems_.clear();
  iconVertexCache_.clear();

  shaders_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
