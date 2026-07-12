// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/TextRenderManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_set>
#include <utility>

namespace noodles {

namespace {

int utf8ToCodepoint(const char* str, int* bytesConsumed) {
  auto c = static_cast<unsigned char>(str[0]);
  if ((c & 0x80) == 0) {
    *bytesConsumed = 1;
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    *bytesConsumed = 2;
    return ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[1]) & 0x3F);
  } else if ((c & 0xF0) == 0xE0) {
    *bytesConsumed = 3;
    return ((c & 0x0F) << 12) | ((static_cast<unsigned char>(str[1]) & 0x3F) << 6) |
        (static_cast<unsigned char>(str[2]) & 0x3F);
  } else if ((c & 0xF8) == 0xF0) {
    *bytesConsumed = 4;
    return ((c & 0x07) << 18) | ((static_cast<unsigned char>(str[1]) & 0x3F) << 12) |
        ((static_cast<unsigned char>(str[2]) & 0x3F) << 6) |
        (static_cast<unsigned char>(str[3]) & 0x3F);
  }
  *bytesConsumed = 1;
  return 0xFFFD;
}

// Emit a filled triangle "caret" (group-fold / title-collapse indicator) as two
// text triangles. Mirrors usd_noodles/textRenderer.py:_generateTriangleCaret:
// the UVs point at the solid centre of the '>' glyph in the MSDF atlas so the
// shader's median() resolves to fully opaque. Returns 1 ("character" of 6
// vertices) so callers can advance textNumChars consistently.
int generateTriangleCaret(
    double x,
    double y,
    double size,
    float depth,
    std::vector<float>& vertexData,
    float nodeIndex,
    bool pointingRight) {
  // Solid-centre UV region of the '>' glyph (atlas pixel coords), spread so
  // fwidth(texCoord) is non-zero.
  const float su0 = 150.0f, sv0 = 400.0f;
  const float su1 = 200.0f, sv1 = 400.0f;
  const float su2 = 175.0f, sv2 = 450.0f;

  double half = size * 0.5;
  double x0, y0, x1, y1, x2, y2;
  if (pointingRight) {
    // ▶ right-pointing
    x0 = x;
    y0 = y - half;
    x1 = x + size;
    y1 = y;
    x2 = x;
    y2 = y + half;
  } else {
    // ▼ down-pointing
    x0 = x;
    y0 = y - half;
    x1 = x + size;
    y1 = y - half;
    x2 = x + half;
    y2 = y + half;
  }

  auto fx0 = static_cast<float>(x0), fy0 = static_cast<float>(y0);
  auto fx1 = static_cast<float>(x1), fy1 = static_cast<float>(y1);
  auto fx2 = static_cast<float>(x2), fy2 = static_cast<float>(y2);

  // Triangle 1: the visible triangle.
  vertexData.insert(vertexData.end(), {fx0, fy0, depth, su0, sv0, nodeIndex});
  vertexData.insert(vertexData.end(), {fx1, fy1, depth, su1, sv1, nodeIndex});
  vertexData.insert(vertexData.end(), {fx2, fy2, depth, su2, sv2, nodeIndex});
  // Triangle 2: degenerate (keeps a uniform 6-verts-per-"char" stride).
  vertexData.insert(vertexData.end(), {fx0, fy0, depth, su0, sv0, nodeIndex});
  vertexData.insert(vertexData.end(), {fx0, fy0, depth, su0, sv0, nodeIndex});
  vertexData.insert(vertexData.end(), {fx0, fy0, depth, su0, sv0, nodeIndex});
  return 1;
}

// Vertex stride: (x, y, z, s, t, nodeIndex); two triangles per glyph.
constexpr int kFloatsPerVertex = 6;
constexpr int kVertsPerChar = 6;
constexpr int kFloatsPerChar = kFloatsPerVertex * kVertsPerChar; // 36
constexpr int kDepthOffset = 2;
constexpr int kNodeIndexOffset = 5;

} // anonymous namespace

TextRenderManager::TextRenderManager() = default;

TextRenderManager::~TextRenderManager() {
  cleanup();
}

void TextRenderManager::initialize(ShaderLibrary* shaders, FontAtlas* fontAtlas) {
  shaders_ = shaders;
  fontAtlas_ = fontAtlas;
  initialized_ = true;
}

double TextRenderManager::calculateTextWidth(const std::string& text, double fontSize) const {
  if (!fontAtlas_) {
    return text.size() * fontSize * 0.5;
  }
  double width = 0.0;
  const auto& glyphs = fontAtlas_->glyphs();
  const char* str = text.c_str();
  int len = static_cast<int>(text.length());
  int pos = 0;
  while (pos < len) {
    int bytesConsumed = 0;
    int unicode = utf8ToCodepoint(str + pos, &bytesConsumed);
    pos += bytesConsumed;
    auto it = glyphs.find(unicode);
    if (it != glyphs.end()) {
      width += it->second.advance;
    }
  }
  return width * fontSize;
}

std::pair<double, int> TextRenderManager::generateTextVertices(
    const std::string& text,
    double cursorX,
    double cursorY,
    float depth,
    double scale,
    std::vector<float>& vertexData,
    float nodeIndex) const {
  if (!fontAtlas_) {
    return {cursorX, 0};
  }

  const auto& glyphs = fontAtlas_->glyphs();
  int charCount = 0;
  const char* str = text.c_str();
  int len = static_cast<int>(text.length());
  int pos = 0;

  while (pos < len) {
    int bytesConsumed = 0;
    int unicode = utf8ToCodepoint(str + pos, &bytesConsumed);
    pos += bytesConsumed;

    auto it = glyphs.find(unicode);
    if (it == glyphs.end()) {
      continue;
    }
    const auto& glyph = it->second;

    auto x0 = static_cast<float>(cursorX + glyph.planeBounds.GetMin()[0] * scale);
    auto y0 = static_cast<float>(cursorY - glyph.planeBounds.GetMax()[1] * scale);
    auto x1 = static_cast<float>(cursorX + glyph.planeBounds.GetMax()[0] * scale);
    auto y1 = static_cast<float>(cursorY - glyph.planeBounds.GetMin()[1] * scale);

    auto s0 = static_cast<float>(glyph.atlasBounds.GetMin()[0]);
    auto t0 = static_cast<float>(glyph.atlasBounds.GetMin()[1]);
    auto s1 = static_cast<float>(glyph.atlasBounds.GetMax()[0]);
    auto t1 = static_cast<float>(glyph.atlasBounds.GetMax()[1]);

    // First triangle
    vertexData.insert(vertexData.end(), {x0, y0, depth, s0, t0, nodeIndex});
    vertexData.insert(vertexData.end(), {x1, y0, depth, s1, t0, nodeIndex});
    vertexData.insert(vertexData.end(), {x0, y1, depth, s0, t1, nodeIndex});

    // Second triangle
    vertexData.insert(vertexData.end(), {x1, y0, depth, s1, t0, nodeIndex});
    vertexData.insert(vertexData.end(), {x1, y1, depth, s1, t1, nodeIndex});
    vertexData.insert(vertexData.end(), {x0, y1, depth, s0, t1, nodeIndex});

    cursorX += glyph.advance * scale;
    charCount++;
  }

  return {cursorX, charCount};
}

void TextRenderManager::setupTextShader(
    const float* projection4x4,
    const std::array<float, 4>& color) {
  auto* shader = shaders_->get("text");
  if (!shader) {
    return;
  }

  shader->use();

  GLint projLoc = shader->getUniformLocation("uProjection");
  if (projLoc >= 0) {
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection4x4);
  }

  GLint atlasLoc = shader->getUniformLocation("uAtlas");
  if (atlasLoc >= 0) {
    glUniform1i(atlasLoc, 0);
  }

  GLint colorLoc = shader->getUniformLocation("uTextColor");
  if (colorLoc >= 0) {
    glUniform4fv(colorLoc, 1, color.data());
  }

  GLint pxRangeLoc = shader->getUniformLocation("uPxRange");
  if (pxRangeLoc >= 0) {
    glUniform1f(pxRangeLoc, static_cast<float>(fontAtlas_->pxRange()));
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  fontAtlas_->bindTexture(0);

  // Atlas is on unit 0; the shared transform texture goes on unit 1.
  if (transformFrame_) {
    transformFrame_->bindToShader(shader, /*textureUnit=*/1);
  }
}

void TextRenderManager::renderText(
    const std::vector<float>& vertexData,
    const float* projection4x4,
    const std::array<float, 4>& color) {
  if (!initialized_ || !shaders_ || !fontAtlas_ || vertexData.empty()) {
    return;
  }

  setupTextShader(projection4x4, color);

  if (textVbo_ == 0) {
    glGenBuffers(1, &textVbo_);
    glGenVertexArrays(1, &textVao_);

    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

    constexpr int stride = 6 * 4;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  } else {
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  int vertexCount = static_cast<int>(vertexData.size()) / 6;
  glBindVertexArray(textVao_);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  glBindVertexArray(0);

  fontAtlas_->unbindTexture();
  shaders_->get("text")->release();
}

void TextRenderManager::renderNodeText(
    const std::vector<float>& vertexData,
    const float* projection4x4,
    const std::array<float, 4>& color,
    bool needsFullUpdate) {
  if (!initialized_ || !shaders_ || !fontAtlas_ || vertexData.empty()) {
    return;
  }

  setupTextShader(projection4x4, color);

  if (needsFullUpdate || nodeTextVbo_ == 0) {
    if (nodeTextVbo_) {
      glDeleteBuffers(1, &nodeTextVbo_);
    }
    if (nodeTextVao_) {
      glDeleteVertexArrays(1, &nodeTextVao_);
    }
    nodeTextVbo_ = 0;
    nodeTextVao_ = 0;

    glGenBuffers(1, &nodeTextVbo_);
    glGenVertexArrays(1, &nodeTextVao_);

    glBindVertexArray(nodeTextVao_);
    glBindBuffer(GL_ARRAY_BUFFER, nodeTextVbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

    constexpr int stride = 6 * 4;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    nodeTextVertexCount_ = static_cast<int>(vertexData.size()) / 6;
  }

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glBindVertexArray(nodeTextVao_);
  glDrawArrays(GL_TRIANGLES, 0, nodeTextVertexCount_);
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);

  fontAtlas_->unbindTexture();
  shaders_->get("text")->release();
}

void TextRenderManager::drawTextVertices(
    const std::vector<float>& vertexData,
    const float* projection4x4,
    const std::array<float, 4>& color,
    bool disableDepth) {
  if (vertexData.empty()) {
    return;
  }

  if (disableDepth) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
  }

  setupTextShader(projection4x4, color);

  if (textVbo_ == 0) {
    glGenBuffers(1, &textVbo_);
    glGenVertexArrays(1, &textVao_);

    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

    constexpr int stride = 6 * 4;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  } else {
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(
        GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  int vertexCount = static_cast<int>(vertexData.size()) / 6;
  glBindVertexArray(textVao_);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  glBindVertexArray(0);

  if (disableDepth) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
  }
}

void TextRenderManager::markNodeTextDirty(bool needsRebuild) {
  if (needsRebuild) {
    nodeTextNeedsRebuild_ = true;
    // Don't just re-concatenate cached blobs — actually re-lay-out every node.
    textVertexCache_.invalidateAll();
  }
  nodeTextDirty_ = true;
}

void TextRenderManager::layoutSingleNode(
    const std::string& nodeId,
    const NodeData& node,
    float shaderNodeIndex,
    float depth,
    const RenderConfig& config,
    std::vector<float>& out) const {
  const double nodeTitleFontSize = config.get("nodeTitleFontSize", 24.0);
  const double nodePinFontSize = config.get("nodePinFontSize", 18.0);
  const double nodePinTypeFontSize = config.get("nodePinTypeFontSize", 14.0);
  const double nodeMarginH = config.get("nodeMarginH", 16.0);
  const double nodeMarginV = config.get("nodeMarginV", 18.0);
  const double nodePortWidth = config.get("nodePortWidth", 16.0);
  const bool isGraffi = config.isGraffiStyle;

  const double ascender = fontAtlas_->ascender();
  const double descender = fontAtlas_->descender();

  // Live moves come from the transform texture, so we lay out at the base frame.
  double baseX = node.position[0];
  double baseY = node.position[1];
  if (transformFrame_) {
    const Vec2d basePos = transformFrame_->basePosition(nodeId);
    baseX = basePos[0];
    baseY = basePos[1];
  }

  // Read layout metrics from the cached values set by calculateNodeSize (single
  // source of truth) so text positions match the node quad rows exactly.
  double titleHeight = node.layoutTitleHeight;
  double portNameHeight = node.layoutPortNameHeight;
  double portTypeHeight = node.layoutPortTypeHeight;
  double portLineHeight = node.layoutPortLineHeight;
  bool hasSchemaType = !node.schemaTypeName.empty();

  double resetX = baseX + nodeMarginH;
  double titleCursorY = baseY + nodeMarginV + ascender * nodeTitleFontSize;

  // Title caret (triangle), reserve icon space, then the title label.
  double titleCaretSize = nodeTitleFontSize * 0.5;
  double titleCaretGap = nodeTitleFontSize * 0.15;
  generateTriangleCaret(
      resetX,
      baseY + titleHeight * 0.5,
      titleCaretSize,
      depth,
      out,
      shaderNodeIndex,
      /*pointingRight=*/node.titleCollapsed);
  double labelStartX = resetX + titleCaretSize + titleCaretGap;
  labelStartX += nodeTitleFontSize * 1.2 + titleCaretGap; // icon reservation

  double titleWidth = calculateTextWidth(node.name, nodeTitleFontSize);
  double titleX = isGraffi ? baseX + (node.size[0] - titleWidth) * 0.5 : labelStartX;
  {
    auto [cx, cc] = generateTextVertices(
        node.name, titleX, titleCursorY, depth, nodeTitleFontSize, out, shaderNodeIndex);
    (void)cx;
    (void)cc;
  }

  // Schema type subtitle (below title; centred for Graffi, else right-aligned).
  if (hasSchemaType) {
    double schemaFontSize = nodeTitleFontSize * RenderConfig::kSchemaTypeFontRatio;
    double schemaTypeY = baseY + nodeMarginV + nodeTitleFontSize * (ascender - descender) +
        ascender * schemaFontSize;
    double schemaTypeWidth = calculateTextWidth(node.schemaTypeName, schemaFontSize);
    double schemaTypeX = isGraffi ? baseX + (node.size[0] - schemaTypeWidth) * 0.5
                                  : baseX + node.size[0] - nodeMarginH - schemaTypeWidth;
    auto [cx, cc] = generateTextVertices(
        node.schemaTypeName, schemaTypeX, schemaTypeY, depth, schemaFontSize, out, shaderNodeIndex);
    (void)cx;
    (void)cc;
  }

  // Graffi: node type centred below the title area.
  if (isGraffi && !node.type.empty()) {
    double nodeTypeY = baseY + node.layoutPortStartY + ascender * nodePinFontSize;
    double nodeTypeWidth = calculateTextWidth(node.type, nodePinTypeFontSize);
    double nodeTypeX = baseX + (node.size[0] - nodeTypeWidth) * 0.5;
    auto [cx, cc] = generateTextVertices(
        node.type, nodeTypeX, nodeTypeY, depth, nodePinTypeFontSize, out, shaderNodeIndex);
    (void)cx;
    (void)cc;
  }

  // Title-collapsed nodes hide all pin rows.
  if (node.titleCollapsed) {
    return;
  }

  double portTextStartY = baseY + node.layoutPortStartY;
  double rowIconSize = portLineHeight * RenderConfig::kRowIconSizeRatio;
  double rowIconGap = nodePinFontSize * RenderConfig::kRowIconGapRatio;
  double rowLabelStartX = resetX + rowIconSize + rowIconGap;

  // Input pins (left-aligned). The vertical anchor is the per-pin band CENTER
  // from the cached layout — identical to the port circle — so text, circles and
  // row stripes all line up. The name (+ optional type) block is centered on it.
  for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
    const std::string& inputPin = node.inputPins[i];
    int rowKind = (i < static_cast<int>(node.inputRowKinds.size())) ? node.inputRowKinds[i] : 0;
    double center = (i < static_cast<int>(node.layoutInputCenterY.size()))
        ? baseY + node.layoutInputCenterY[i]
        : portTextStartY;

    if (rowKind == 1 || rowKind == 2) {
      double caretSize = nodePinFontSize * 0.875;
      double caretGap = nodePinFontSize * 0.15;
      generateTriangleCaret(
          resetX,
          center,
          caretSize,
          depth,
          out,
          shaderNodeIndex,
          /*pointingRight=*/rowKind == 1);
      double labelY = center - portNameHeight * 0.5 + ascender * nodePinFontSize;
      auto [cx, cc] = generateTextVertices(
          inputPin,
          resetX + caretSize + caretGap,
          labelY,
          depth,
          nodePinFontSize,
          out,
          shaderNodeIndex);
      (void)cx;
      (void)cc;
      continue;
    }

    auto ptIt = node.inputPinTypes.find(inputPin);
    std::string portType = (ptIt != node.inputPinTypes.end()) ? ptIt->second : node.type;
    // When pin-type labels are hidden, treat the row as type-less: the block
    // centers the name alone and no "(type)" glyphs are emitted (rows are also
    // shrunk in calculateNodeSize, which zeros portTypeHeight).
    bool hasType = !portType.empty() && config.showPinTypeLabels;
    double blockTop = center - (portNameHeight + (hasType ? portTypeHeight : 0.0)) * 0.5;
    double labelY = blockTop + ascender * nodePinFontSize;

    std::string displayLabel = inputPin;
    double labelX = rowLabelStartX;
    if (rowKind == 3) {
      labelX = rowLabelStartX + nodePortWidth;
      auto colonPos = inputPin.find(':');
      if (colonPos != std::string::npos) {
        displayLabel = inputPin.substr(colonPos + 1);
      }
    }
    {
      auto [cx, cc] = generateTextVertices(
          displayLabel, labelX, labelY, depth, nodePinFontSize, out, shaderNodeIndex);
      (void)cx;
      (void)cc;
    }

    if (hasType) {
      std::string pinTypeText = "(" + portType + ")";
      double typeY = blockTop + portNameHeight + ascender * nodePinTypeFontSize;
      double typeX = (rowKind == 3) ? rowLabelStartX + nodePortWidth : rowLabelStartX;
      auto [cx, cc] = generateTextVertices(
          pinTypeText, typeX, typeY, depth, nodePinTypeFontSize, out, shaderNodeIndex);
      (void)cx;
      (void)cc;
    }
  }

  // Output pins (labels right-aligned; group headers left-aligned like inputs).
  for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
    const std::string& outputPin = node.outputPins[i];
    int rowKind = (i < static_cast<int>(node.outputRowKinds.size())) ? node.outputRowKinds[i] : 0;
    double center = (i < static_cast<int>(node.layoutOutputCenterY.size()))
        ? baseY + node.layoutOutputCenterY[i]
        : portTextStartY;

    if (rowKind == 1 || rowKind == 2) {
      // A namespaced group present on both edges (e.g. "skel": dual attributes
      // on the input side, relationships on the output side) shares ONE row: the
      // input header already drew this caret/label at the same slot, so skip the
      // output header's text to avoid a doubled header. The row still exists on
      // the output side so a folded group keeps its right-edge aggregate port.
      int outSlot = (i < static_cast<int>(node.outputRowSlots.size())) ? node.outputRowSlots[i] : i;
      bool mirroredByInputHeader = false;
      for (int j = 0; j < static_cast<int>(node.inputPins.size()); ++j) {
        int inKind = (j < static_cast<int>(node.inputRowKinds.size())) ? node.inputRowKinds[j] : 0;
        if ((inKind == 1 || inKind == 2) && node.inputPins[j] == outputPin) {
          int inSlot =
              (j < static_cast<int>(node.inputRowSlots.size())) ? node.inputRowSlots[j] : j;
          if (inSlot == outSlot) {
            mirroredByInputHeader = true;
            break;
          }
        }
      }
      if (mirroredByInputHeader) {
        continue;
      }
      double caretSize = nodePinFontSize * 0.875;
      double caretGap = nodePinFontSize * 0.15;
      generateTriangleCaret(
          resetX,
          center,
          caretSize,
          depth,
          out,
          shaderNodeIndex,
          /*pointingRight=*/rowKind == 1);
      double labelY = center - portNameHeight * 0.5 + ascender * nodePinFontSize;
      auto [cx, cc] = generateTextVertices(
          outputPin,
          resetX + caretSize + caretGap,
          labelY,
          depth,
          nodePinFontSize,
          out,
          shaderNodeIndex);
      (void)cx;
      (void)cc;
      continue;
    }

    auto ptItOut = node.outputPinTypes.find(outputPin);
    std::string portTypeOut = (ptItOut != node.outputPinTypes.end()) ? ptItOut->second : node.type;
    bool hasType = !portTypeOut.empty() && config.showPinTypeLabels;
    double blockTop = center - (portNameHeight + (hasType ? portTypeHeight : 0.0)) * 0.5;
    double labelY = blockTop + ascender * nodePinFontSize;
    std::string displayLabel = outputPin;
    if (rowKind == 3) {
      auto colonPos = outputPin.find(':');
      if (colonPos != std::string::npos) {
        displayLabel = outputPin.substr(colonPos + 1);
      }
    }

    std::string pinTypeText = hasType ? "(" + portTypeOut + ")" : "";
    double typeY = blockTop + portNameHeight + ascender * nodePinTypeFontSize;

    if (node.relationshipOutputPins.count(outputPin) > 0) {
      // Relationship rows are left-aligned — matching the row's left
      // double-arrow icon and the input-pin labels — instead of right-aligned
      // against the output port.
      double labelX = (rowKind == 3) ? rowLabelStartX + nodePortWidth : rowLabelStartX;
      auto [cx, cc] = generateTextVertices(
          displayLabel, labelX, labelY, depth, nodePinFontSize, out, shaderNodeIndex);
      (void)cx;
      (void)cc;
      if (hasType) {
        auto [tcx, tcc] = generateTextVertices(
            pinTypeText, labelX, typeY, depth, nodePinTypeFontSize, out, shaderNodeIndex);
        (void)tcx;
        (void)tcc;
      }
      continue;
    }

    double rightMargin = nodeMarginH;
    if (rowKind == 3) {
      rightMargin += nodePortWidth;
    }
    double stringWidth = calculateTextWidth(displayLabel, nodePinFontSize);
    {
      auto [cx, cc] = generateTextVertices(
          displayLabel,
          baseX + node.size[0] - rightMargin - stringWidth,
          labelY,
          depth,
          nodePinFontSize,
          out,
          shaderNodeIndex);
      (void)cx;
      (void)cc;
    }

    if (hasType) {
      double typeWidth = calculateTextWidth(pinTypeText, nodePinTypeFontSize);
      auto [cx, cc] = generateTextVertices(
          pinTypeText,
          baseX + node.size[0] - rightMargin - typeWidth,
          typeY,
          depth,
          nodePinTypeFontSize,
          out,
          shaderNodeIndex);
      (void)cx;
      (void)cc;
    }
  }
}

std::vector<float> TextRenderManager::assembleNodeTextBuffer(
    const std::vector<NodeTextAssemblyItem>& items,
    std::vector<int>& outStarts,
    std::vector<int>& outCharCounts) {
  outStarts.clear();
  outCharCounts.clear();
  outStarts.reserve(items.size());
  outCharCounts.reserve(items.size());

  size_t total = 0;
  for (const auto& item : items) {
    if (item.vertices != nullptr) {
      total += item.vertices->size();
    }
  }

  std::vector<float> buffer;
  buffer.reserve(total);
  for (const auto& item : items) {
    int start = static_cast<int>(buffer.size());
    int floatCount = (item.vertices != nullptr) ? static_cast<int>(item.vertices->size()) : 0;
    if (floatCount > 0) {
      buffer.insert(buffer.end(), item.vertices->begin(), item.vertices->end());
      // Stamp current depth/nodeIndex so cached blobs stay correct after z-order
      // or slot changes without needing a full re-layout.
      for (int j = start; j + kFloatsPerVertex <= start + floatCount; j += kFloatsPerVertex) {
        buffer[j + kDepthOffset] = item.depth;
        buffer[j + kNodeIndexOffset] = item.nodeIndex;
      }
    }
    outStarts.push_back(start);
    outCharCounts.push_back(floatCount / kFloatsPerChar);
  }
  return buffer;
}

std::vector<float> TextRenderManager::generateNodeTextVertices(
    std::unordered_map<std::string, NodeData>& nodes,
    const RenderConfig& config) {
  nodeTextSlices_.clear();

  // The shared frame owns base positions / move offsets / slot assignment.
  transformFrame_->syncFromNodes(nodes);

  // Prune cached text blobs for deleted nodes.
  std::unordered_set<std::string> liveIds;
  liveIds.reserve(nodes.size());
  for (const auto& entry : nodes) {
    liveIds.insert(entry.first);
  }
  textVertexCache_.retainNodes(liveIds);

  // Back-to-front so text depth matches each node's background quad (shared
  // ordering with the node-quad path keeps the two layers in lockstep).
  auto sortedNodes = sortNodesByZOrder(nodes);

  for (auto& [nodeIdPtr, nodePtr] : sortedNodes) {
    const std::string& nodeId = *nodeIdPtr;
    NodeData& node = *nodePtr;
    TextLayoutSignature signature = makeTextLayoutSignature(node, config);
    if (textVertexCache_.isDirty(nodeId, signature)) {
      float shaderNodeIndex = transformFrame_->shaderIndex(nodeId);
      auto depth = static_cast<float>(RenderConfig::contentDepth(node.zOrder));
      std::vector<float> verts;
      layoutSingleNode(nodeId, node, shaderNodeIndex, depth, config, verts);
      textVertexCache_.update(nodeId, std::move(verts), signature);
    }
  }

  std::vector<NodeTextAssemblyItem> items;
  items.reserve(sortedNodes.size());
  for (const auto& [nodeIdPtr, nodePtr] : sortedNodes) {
    items.push_back(
        {&textVertexCache_.getVertices(*nodeIdPtr),
         static_cast<float>(RenderConfig::contentDepth(nodePtr->zOrder)),
         transformFrame_->shaderIndex(*nodeIdPtr)});
  }

  std::vector<int> starts;
  std::vector<int> charCounts;
  nodeTextVertexData_ = assembleNodeTextBuffer(items, starts, charCounts);

  // nodeTextSlices_ is authoritative (patchNodeTextDepth reads it); the
  // NodeData fields are a mirror kept for the boost.python binding.
  for (size_t i = 0; i < sortedNodes.size(); ++i) {
    nodeTextSlices_[*sortedNodes[i].first] = {starts[i], charCounts[i]};
    sortedNodes[i].second->textStartIndex = starts[i];
    sortedNodes[i].second->textNumChars = charCounts[i];
  }

  nodeTextNeedsRebuild_ = false;
  return nodeTextVertexData_;
}

void TextRenderManager::resetPositionCaches() {
  nodeTextSlices_.clear();
  // (Re)load changes the node set, so stale cached geometry can't be reused.
  textVertexCache_.clear();
  nodeTextNeedsRebuild_ = true;
}

bool TextRenderManager::patchNodeTextDepth(const std::string& nodeId, int zOrder) {
  auto it = nodeTextSlices_.find(nodeId);
  if (it == nodeTextSlices_.end()) {
    return false;
  }
  int start = it->second.first;
  int numChars = it->second.second;
  int end = start + numChars * kFloatsPerChar;
  if (numChars <= 0 || start < 0 || end > static_cast<int>(nodeTextVertexData_.size())) {
    return false;
  }
  auto depth = static_cast<float>(RenderConfig::contentDepth(zOrder));
  for (int j = start + kDepthOffset; j < end; j += kFloatsPerVertex) {
    nodeTextVertexData_[j] = depth;
  }
  nodeTextDirty_ = true;
  return true;
}

void TextRenderManager::computeVisibleTextRanges(
    const std::unordered_map<std::string, NodeData>& nodes,
    const std::unordered_map<std::string, std::pair<int, int>>& slices,
    double vpMinX,
    double vpMinY,
    double vpMaxX,
    double vpMaxY,
    std::vector<GLint>& outFirsts,
    std::vector<GLsizei>& outCounts,
    const NodeTransformFrame* transformFrame) {
  outFirsts.clear();
  outCounts.clear();
  for (const auto& [nodeId, node] : nodes) {
    auto it = slices.find(nodeId);
    if (it == slices.end()) {
      continue;
    }
    int numChars = it->second.second;
    if (numChars <= 0) {
      continue; // collapsed / empty node: nothing to draw
    }
    // Cull at the SAME world position the GPU draws the glyphs at — the
    // transform frame's base + live move offset — not the GraphModel snapshot,
    // which is not re-synced on a drag and would strand a dragged node's text.
    // NodeTransformFrame::liveOrigin centralizes that rule (shared with the icon
    // cull). All of a node's text lays out inside its box, so the node AABB is a
    // safe (slightly conservative) bound: a node fully outside is skipped, an
    // intersecting one is kept in full.
    const Vec2d origin = (transformFrame != nullptr)
        ? transformFrame->liveOrigin(nodeId, node.position)
        : node.position;
    double minX = origin[0];
    double minY = origin[1];
    double maxX = minX + node.size[0];
    double maxY = minY + node.size[1];
    if (maxX < vpMinX || minX > vpMaxX || maxY < vpMinY || minY > vpMaxY) {
      continue;
    }
    outFirsts.push_back(static_cast<GLint>(it->second.first / kFloatsPerVertex));
    outCounts.push_back(static_cast<GLsizei>(numChars * kVertsPerChar));
  }
}

bool TextRenderManager::renderNodeTextFull(
    std::unordered_map<std::string, NodeData>& nodes,
    const float* projection4x4,
    float zoom,
    bool textChanged,
    const RenderConfig& config,
    double panX,
    double panY,
    double viewportWidth,
    double viewportHeight,
    bool cullOffscreen) {
  if (nodes.empty() || !initialized_ || !shaders_ || !fontAtlas_ || !transformFrame_) {
    return false;
  }

  bool needsFullUpdate = textChanged || nodeTextNeedsRebuild_;

  if (needsFullUpdate) {
    generateNodeTextVertices(nodes, config);
    nodeTextNeedsRebuild_ = false;
  }

  if (nodeTextVertexData_.empty()) {
    // Nothing to draw (every node folded / culled off-screen / no text) is a
    // SUCCESSFUL frame, not a failure -- the real failure cases are handled by
    // the guard above. Return true so the caller clears its textChanged flag;
    // returning false here left textChanged stuck on, which re-ran a full
    // per-node size + text relayout (and syncNodesFromModels) on every
    // subsequent frame, e.g. throughout a pan.
    return true;
  }

  std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
  setupTextShader(projection4x4, color);

  transformFrame_->upload();

  if (needsFullUpdate || nodeTextVbo_ == 0) {
    if (nodeTextVbo_) {
      glDeleteBuffers(1, &nodeTextVbo_);
    }
    if (nodeTextVao_) {
      glDeleteVertexArrays(1, &nodeTextVao_);
    }
    nodeTextVbo_ = 0;
    nodeTextVao_ = 0;

    glGenBuffers(1, &nodeTextVbo_);
    glGenVertexArrays(1, &nodeTextVao_);

    glBindVertexArray(nodeTextVao_);
    glBindBuffer(GL_ARRAY_BUFFER, nodeTextVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        nodeTextVertexData_.size() * sizeof(float),
        nodeTextVertexData_.data(),
        GL_DYNAMIC_DRAW);

    constexpr int stride = 6 * 4;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    nodeTextVertexCount_ = static_cast<int>(nodeTextVertexData_.size()) / 6;
  } else if (nodeTextDirty_) {
    glBindBuffer(GL_ARRAY_BUFFER, nodeTextVbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, nodeTextVertexData_.size() * sizeof(float), nodeTextVertexData_.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    nodeTextDirty_ = false;
  }

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glBindVertexArray(nodeTextVao_);
  // Off-screen culling: draw only the visible nodes' glyph ranges out of the
  // (otherwise unchanged) buffer, so pan/zoom never rebuilds and a huge graph
  // only pays for on-screen text. The world-space viewport rect mirrors
  // _worldSpaceProjectionMatrix's ortho(panX, panX+w/zoom, panY+h/zoom, panY).
  bool culled = false;
  if (cullOffscreen && zoom > 0.0f && viewportWidth > 0.0 && viewportHeight > 0.0) {
    double vpMinX = panX;
    double vpMinY = panY;
    double vpMaxX = panX + viewportWidth / static_cast<double>(zoom);
    double vpMaxY = panY + viewportHeight / static_cast<double>(zoom);
    std::vector<GLint> firsts;
    std::vector<GLsizei> counts;
    // Pass the transform frame so the cull uses each node's live (base + offset)
    // position — the same place the glyphs draw — instead of the stale snapshot.
    computeVisibleTextRanges(
        nodes, nodeTextSlices_, vpMinX, vpMinY, vpMaxX, vpMaxY, firsts, counts, transformFrame_);
    glMultiDrawArrays(
        GL_TRIANGLES, firsts.data(), counts.data(), static_cast<GLsizei>(firsts.size()));
    culled = true;
  }
  if (!culled) {
    glDrawArrays(GL_TRIANGLES, 0, nodeTextVertexCount_);
  }
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);

  fontAtlas_->unbindTexture();
  shaders_->get("text")->release();

  return true;
}

void TextRenderManager::cleanup() {
  if (textVbo_) {
    glDeleteBuffers(1, &textVbo_);
    textVbo_ = 0;
  }
  if (textVao_) {
    glDeleteVertexArrays(1, &textVao_);
    textVao_ = 0;
  }
  if (nodeTextVbo_) {
    glDeleteBuffers(1, &nodeTextVbo_);
    nodeTextVbo_ = 0;
  }
  if (nodeTextVao_) {
    glDeleteVertexArrays(1, &nodeTextVao_);
    nodeTextVao_ = 0;
  }
  // transformFrame_ is owned elsewhere (shared); don't delete its texture here.

  nodeTextVertexData_.clear();
  nodeTextSlices_.clear();
  textVertexCache_.clear();

  shaders_ = nullptr;
  fontAtlas_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
