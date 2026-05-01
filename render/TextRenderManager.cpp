// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/TextRenderManager.h"

#include <array>
#include <cstring>

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

} // anonymous namespace

TextRenderManager::TextRenderManager() = default;

TextRenderManager::~TextRenderManager() {
  cleanup();
}

void TextRenderManager::initialize(ShaderLibrary* shaders, FontAtlas* fontAtlas) {
  shaders_ = shaders;
  fontAtlas_ = fontAtlas;
  initialized_ = true;
  createTransformTexture();
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

void TextRenderManager::createTransformTexture() {
  if (transformTexture_) {
    glDeleteTextures(1, &transformTexture_);
  }

  glGenTextures(1, &transformTexture_);
  glBindTexture(GL_TEXTURE_1D, transformTexture_);

  std::vector<float> data(static_cast<size_t>(transformTextureSize_) * 2, 0.0f);
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RG32F, transformTextureSize_, 0, GL_RG, GL_FLOAT, data.data());

  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_1D, 0);
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

  GLint texSizeLoc = shader->getUniformLocation("uTransformTextureSize");
  if (texSizeLoc >= 0) {
    glUniform1f(texSizeLoc, static_cast<float>(transformTextureSize_));
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  fontAtlas_->bindTexture(0);

  GLint transformsLoc = shader->getUniformLocation("uNodeTransforms");
  if (transformsLoc >= 0 && transformTexture_) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, transformTexture_);
    glUniform1i(transformsLoc, 1);
    glActiveTexture(GL_TEXTURE0);
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

void TextRenderManager::updateTransforms(
    const std::unordered_map<std::string, int>& nodeIdToIndex,
    const std::unordered_map<std::string, std::pair<float, float>>& transforms) {
  if (!transformTexture_) {
    createTransformTexture();
  }

  std::vector<float> data(static_cast<size_t>(transformTextureSize_) * 2, 0.0f);
  for (const auto& [nodeId, nodeIdx] : nodeIdToIndex) {
    auto tIt = transforms.find(nodeId);
    if (tIt != transforms.end() && nodeIdx < transformTextureSize_) {
      data[nodeIdx * 2] = tIt->second.first;
      data[nodeIdx * 2 + 1] = tIt->second.second;
    }
  }

  glBindTexture(GL_TEXTURE_1D, transformTexture_);
  glTexSubImage1D(GL_TEXTURE_1D, 0, 0, transformTextureSize_, GL_RG, GL_FLOAT, data.data());
  glBindTexture(GL_TEXTURE_1D, 0);
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
  }
  nodeTextDirty_ = true;
}

void TextRenderManager::updateNodeTextPosition(const std::string& nodeId, double dx, double dy) {
  auto it = nodeTransforms_.find(nodeId);
  if (it == nodeTransforms_.end()) {
    return;
  }
  it->second.first += static_cast<float>(dx);
  it->second.second += static_cast<float>(dy);
}

std::vector<float> TextRenderManager::generateNodeTextVertices(
    std::unordered_map<std::string, NodeData>& nodes,
    const RenderConfig& config) {
  nodeTextVertexData_.clear();
  nodeIdToIndex_.clear();

  int nodeIndex = 0;
  for (auto& [nodeId, node] : nodes) {
    nodeIdToIndex_[nodeId] = nodeIndex;
    if (nodeBasePositions_.find(nodeId) == nodeBasePositions_.end()) {
      nodeBasePositions_[nodeId] = {node.position[0], node.position[1]};
    }
    if (nodeTransforms_.find(nodeId) == nodeTransforms_.end()) {
      auto& basePos = nodeBasePositions_[nodeId];
      nodeTransforms_[nodeId] = {
          static_cast<float>(node.position[0] - basePos.first),
          static_cast<float>(node.position[1] - basePos.second)};
    }
    nodeIndex++;
  }

  float depth = 0.0001f;
  float depthIncrement = 0.0001f;

  for (auto& [nodeId, node] : nodes) {
    auto& basePos = nodeBasePositions_[nodeId];
    double baseX = basePos.first;
    double baseY = basePos.second;
    auto shaderNodeIndex = static_cast<float>(nodeIdToIndex_[nodeId]);

    double nodeTitleFontSize = 0.0;
    double nodePinFontSize = 0.0;
    double nodePinTypeFontSize = 0.0;
    double nodeMarginH = 0.0;
    double nodeMarginV = 0.0;
    double nodePortSpacing = 0.0;
    bool outputPortsTopToBottom = false;
    bool isGraffi = false;

    nodeTitleFontSize = config.get("nodeTitleFontSize", 24.0);
    nodePinFontSize = config.get("nodePinFontSize", 18.0);
    nodePinTypeFontSize = config.get("nodePinTypeFontSize", 14.0);
    nodeMarginH = config.get("nodeMarginH", 16.0);
    nodeMarginV = config.get("nodeMarginV", 18.0);
    nodePortSpacing = config.get("nodePortSpacing", 1.0);
    outputPortsTopToBottom = config.outputPortsTopToBottom;
    isGraffi = config.isGraffiStyle;

    double titleHeight =
        nodeTitleFontSize * (fontAtlas_->ascender() - fontAtlas_->descender()) + nodeMarginV * 2.0;

    double portNameHeight = nodePinFontSize * fontAtlas_->lineHeight();
    double portTypeHeight = nodePinTypeFontSize * fontAtlas_->lineHeight();
    double portLineHeight = portNameHeight + portTypeHeight + nodePortSpacing;

    double resetX = baseX + nodeMarginH;
    double cursorX = resetX;
    double cursorY = baseY + nodeMarginV + fontAtlas_->ascender() * nodeTitleFontSize;

    node.textStartIndex = static_cast<int>(nodeTextVertexData_.size());
    node.textNumChars = 0;

    // Title - center for Graffi, left-align otherwise
    double titleWidth = calculateTextWidth(node.name, nodeTitleFontSize);
    if (isGraffi) {
      cursorX = baseX + (node.size[0] - titleWidth) * 0.5;
    } else {
      cursorX = resetX;
    }

    auto [cx1, cc1] = generateTextVertices(
        node.name,
        cursorX,
        cursorY,
        depth,
        nodeTitleFontSize,
        nodeTextVertexData_,
        shaderNodeIndex);
    node.textNumChars += cc1;

    // Graffi: show node type centered below title
    if (isGraffi && !node.type.empty()) {
      double nodeTypeY =
          baseY + titleHeight + nodeMarginV + fontAtlas_->ascender() * nodePinFontSize;
      double nodeTypeWidth = calculateTextWidth(node.type, nodePinTypeFontSize);
      double nodeTypeX = baseX + (node.size[0] - nodeTypeWidth) * 0.5;
      auto [cx2, cc2] = generateTextVertices(
          node.type,
          nodeTypeX,
          nodeTypeY,
          depth,
          nodePinTypeFontSize,
          nodeTextVertexData_,
          shaderNodeIndex);
      node.textNumChars += cc2;
    }

    // Input pins
    double portTextStartY = baseY + titleHeight + nodeMarginV;
    cursorX = resetX;

    double nodePortWidth = config.get("nodePortWidth", 16.0);

    for (int i = 0; i < static_cast<int>(node.inputPins.size()); ++i) {
      const auto& inputPin = node.inputPins[i];

      // Determine row kind (0=normal, 1=folded header, 2=unfolded header, 3=child)
      int rowKind = (i < static_cast<int>(node.inputRowKinds.size())) ? node.inputRowKinds[i] : 0;

      double portCenterOffset = portNameHeight * 0.5;
      double textCenterOffset =
          (fontAtlas_->ascender() + fontAtlas_->descender()) * nodePinFontSize * 0.5;
      cursorY = portTextStartY + i * portLineHeight + portCenterOffset + textCenterOffset;
      cursorX = resetX;

      if (rowKind == 1 || rowKind == 2) {
        // Group header: render caret + pin name, skip type label
        std::string caretLabel =
            (rowKind == 1) ? std::string("> ") + inputPin : std::string("v ") + inputPin;
        auto [cx3, cc3] = generateTextVertices(
            caretLabel,
            cursorX,
            cursorY,
            depth,
            nodePinFontSize,
            nodeTextVertexData_,
            shaderNodeIndex);
        node.textNumChars += cc3;
      } else {
        // Normal or child pin
        std::string displayLabel = inputPin;
        if (rowKind == 3) {
          // Child: indent and strip namespace prefix
          cursorX = resetX + nodePortWidth;
          auto colonPos = inputPin.find(':');
          if (colonPos != std::string::npos) {
            displayLabel = inputPin.substr(colonPos + 1);
          }
        }

        auto [cx3, cc3] = generateTextVertices(
            displayLabel,
            cursorX,
            cursorY,
            depth,
            nodePinFontSize,
            nodeTextVertexData_,
            shaderNodeIndex);
        node.textNumChars += cc3;

        // Type label (skip for group headers)
        auto ptIt = node.inputPinTypes.find(inputPin);
        std::string portType = (ptIt != node.inputPinTypes.end()) ? ptIt->second : node.type;
        if (!portType.empty()) {
          std::string pinTypeText = "(" + portType + ")";
          double typeCursorY = portTextStartY + i * portLineHeight + portNameHeight +
              fontAtlas_->ascender() * nodePinTypeFontSize;
          double typeCursorX = (rowKind == 3) ? resetX + nodePortWidth : resetX;
          auto [cx4, cc4] = generateTextVertices(
              pinTypeText,
              typeCursorX,
              typeCursorY,
              depth,
              nodePinTypeFontSize,
              nodeTextVertexData_,
              shaderNodeIndex);
          node.textNumChars += cc4;
        }
      }
    }

    // Output pins
    double outputPortStartY = 0.0;
    if (outputPortsTopToBottom) {
      outputPortStartY = portTextStartY;
    } else {
      outputPortStartY =
          portTextStartY + static_cast<double>(node.inputPins.size()) * portLineHeight;
    }

    for (int i = 0; i < static_cast<int>(node.outputPins.size()); ++i) {
      const auto& outputPin = node.outputPins[i];

      // Determine row kind
      int rowKind = (i < static_cast<int>(node.outputRowKinds.size())) ? node.outputRowKinds[i] : 0;

      double portCenterOffset = portNameHeight * 0.5;
      double textCenterOffset =
          (fontAtlas_->ascender() + fontAtlas_->descender()) * nodePinFontSize * 0.5;
      cursorY = outputPortStartY + i * portLineHeight + portCenterOffset + textCenterOffset;

      if (rowKind == 1 || rowKind == 2) {
        // Group header: render caret + pin name, right-aligned, skip type label
        std::string caretLabel =
            (rowKind == 1) ? std::string("> ") + outputPin : std::string("v ") + outputPin;
        double stringWidth = calculateTextWidth(caretLabel, nodePinFontSize);
        cursorX = baseX + node.size[0] - nodeMarginH - stringWidth;
        auto [cx5, cc5] = generateTextVertices(
            caretLabel,
            cursorX,
            cursorY,
            depth,
            nodePinFontSize,
            nodeTextVertexData_,
            shaderNodeIndex);
        node.textNumChars += cc5;
      } else {
        // Normal or child pin
        std::string displayLabel = outputPin;
        double rightMargin = nodeMarginH;
        if (rowKind == 3) {
          // Child: strip namespace prefix
          auto colonPos = outputPin.find(':');
          if (colonPos != std::string::npos) {
            displayLabel = outputPin.substr(colonPos + 1);
          }
          rightMargin = nodeMarginH + nodePortWidth;
        }

        double stringWidth = calculateTextWidth(displayLabel, nodePinFontSize);
        cursorX = baseX + node.size[0] - rightMargin - stringWidth;

        auto [cx5, cc5] = generateTextVertices(
            displayLabel,
            cursorX,
            cursorY,
            depth,
            nodePinFontSize,
            nodeTextVertexData_,
            shaderNodeIndex);
        node.textNumChars += cc5;

        // Type label (skip for group headers)
        auto ptIt = node.outputPinTypes.find(outputPin);
        std::string portType = (ptIt != node.outputPinTypes.end()) ? ptIt->second : node.type;
        if (!portType.empty()) {
          std::string pinTypeText = "(" + portType + ")";
          double typeWidth = calculateTextWidth(pinTypeText, nodePinTypeFontSize);
          double typeCursorX = baseX + node.size[0] - rightMargin - typeWidth;
          double typeCursorY = outputPortStartY + i * portLineHeight + portNameHeight +
              fontAtlas_->ascender() * nodePinTypeFontSize;
          auto [cx6, cc6] = generateTextVertices(
              pinTypeText,
              typeCursorX,
              typeCursorY,
              depth,
              nodePinTypeFontSize,
              nodeTextVertexData_,
              shaderNodeIndex);
          node.textNumChars += cc6;
        }
      }
    }

    depth += depthIncrement;
  }

  nodeTextNeedsRebuild_ = false;
  return nodeTextVertexData_;
}

void TextRenderManager::updateTransformTexture() {
  if (!transformTexture_) {
    createTransformTexture();
  }

  std::vector<float> data(static_cast<size_t>(transformTextureSize_) * 2, 0.0f);
  for (const auto& [nodeId, nodeIdx] : nodeIdToIndex_) {
    auto tIt = nodeTransforms_.find(nodeId);
    if (tIt != nodeTransforms_.end() && nodeIdx < transformTextureSize_) {
      data[nodeIdx * 2] = tIt->second.first;
      data[nodeIdx * 2 + 1] = tIt->second.second;
    }
  }

  glBindTexture(GL_TEXTURE_1D, transformTexture_);
  glTexSubImage1D(GL_TEXTURE_1D, 0, 0, transformTextureSize_, GL_RG, GL_FLOAT, data.data());
  glBindTexture(GL_TEXTURE_1D, 0);
}

bool TextRenderManager::renderNodeTextFull(
    std::unordered_map<std::string, NodeData>& nodes,
    const float* projection4x4,
    float /*zoom*/,
    bool textChanged,
    const RenderConfig& config) {
  if (nodes.empty() || !initialized_ || !shaders_ || !fontAtlas_) {
    return false;
  }

  bool needsFullUpdate = textChanged || nodeTextNeedsRebuild_;

  if (needsFullUpdate) {
    generateNodeTextVertices(nodes, config);
    nodeTextNeedsRebuild_ = false;
  }

  if (nodeTextVertexData_.empty()) {
    return false;
  }

  std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
  setupTextShader(projection4x4, color);

  updateTransformTexture();

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
  glDrawArrays(GL_TRIANGLES, 0, nodeTextVertexCount_);
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
  if (transformTexture_) {
    glDeleteTextures(1, &transformTexture_);
    transformTexture_ = 0;
  }

  nodeTextVertexData_.clear();
  nodeIdToIndex_.clear();
  nodeTransforms_.clear();
  nodeBasePositions_.clear();

  shaders_ = nullptr;
  fontAtlas_ = nullptr;
  initialized_ = false;
}

} // namespace noodles
