// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/TextLayout.h"

#include <cstring>

namespace noodles {

int TextLayout::_Utf8ToCodepoint(const char* str, int* bytesConsumed) {
  // Decode UTF-8 to Unicode codepoint
  auto c = static_cast<unsigned char>(str[0]);

  if ((c & 0x80) == 0) { // Single byte (ASCII)
    *bytesConsumed = 1;
    return c;
  } else if ((c & 0xE0) == 0xC0) { // Two bytes
    *bytesConsumed = 2;
    return ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[1]) & 0x3F);
  } else if ((c & 0xF0) == 0xE0) { // Three bytes
    *bytesConsumed = 3;
    return ((c & 0x0F) << 12) | ((static_cast<unsigned char>(str[1]) & 0x3F) << 6) |
        (static_cast<unsigned char>(str[2]) & 0x3F);
  } else if ((c & 0xF8) == 0xF0) { // Four bytes
    *bytesConsumed = 4;
    return ((c & 0x07) << 18) | ((static_cast<unsigned char>(str[1]) & 0x3F) << 12) |
        ((static_cast<unsigned char>(str[2]) & 0x3F) << 6) |
        (static_cast<unsigned char>(str[3]) & 0x3F);
  }

  // Invalid UTF-8, skip one byte
  *bytesConsumed = 1;
  return 0xFFFD; // Replacement character
}

double TextLayout::CalculateTextWidth(
    const std::string& text,
    double fontSize,
    const std::unordered_map<int, GlyphMetrics>& glyphMap) {
  if (text.empty() || glyphMap.empty()) {
    // Fallback: approximate width based on character count
    return static_cast<double>(text.length()) * fontSize * 0.5;
  }

  double width = 0.0;
  const char* str = text.c_str();
  int len = static_cast<int>(text.length());
  int pos = 0;

  while (pos < len) {
    int bytesConsumed = 0;
    int codepoint = _Utf8ToCodepoint(str + pos, &bytesConsumed);
    pos += bytesConsumed;

    auto it = glyphMap.find(codepoint);
    if (it != glyphMap.end()) {
      width += it->second.advance;
    }
  }

  return width * fontSize;
}

TextVertexResult TextLayout::GenerateTextVertices(
    const std::string& text,
    double cursorX,
    double cursorY,
    double depth,
    double scale,
    const std::unordered_map<int, GlyphMetrics>& glyphMap,
    double /*ascender*/,
    double nodeIndex,
    std::vector<float>& vertexData) {
  if (text.empty() || glyphMap.empty()) {
    return TextVertexResult(cursorX, 0);
  }

  // (6 floats per vertex, 6 vertices per char)
  vertexData.reserve(vertexData.size() + text.length() * 6 * 6);

  const char* str = text.c_str();
  int len = static_cast<int>(text.length());
  int pos = 0;
  int charCount = 0;
  double currentX = cursorX;

  auto depthF = static_cast<float>(depth);
  auto nodeIndexF = static_cast<float>(nodeIndex);

  while (pos < len) {
    int bytesConsumed = 0;
    int codepoint = _Utf8ToCodepoint(str + pos, &bytesConsumed);
    pos += bytesConsumed;

    auto it = glyphMap.find(codepoint);
    if (it == glyphMap.end()) {
      continue;
    }

    const GlyphMetrics& glyph = it->second;

    // Calculate vertex positions
    auto x0 = static_cast<float>(currentX + glyph.planeBounds.GetMin()[0] * scale);
    auto y0 = static_cast<float>(cursorY - glyph.planeBounds.GetMax()[1] * scale);
    auto x1 = static_cast<float>(currentX + glyph.planeBounds.GetMax()[0] * scale);
    auto y1 = static_cast<float>(cursorY - glyph.planeBounds.GetMin()[1] * scale);

    // Atlas UV coordinates
    auto s0 = static_cast<float>(glyph.atlasBounds.GetMin()[0]);
    auto t0 = static_cast<float>(glyph.atlasBounds.GetMin()[1]);
    auto s1 = static_cast<float>(glyph.atlasBounds.GetMax()[0]);
    auto t1 = static_cast<float>(glyph.atlasBounds.GetMax()[1]);

    // Vertex format: x, y, z, s, t, nodeIndex (6 floats per vertex)
    // First triangle (CCW winding)
    vertexData.push_back(x0);
    vertexData.push_back(y0);
    vertexData.push_back(depthF);
    vertexData.push_back(s0);
    vertexData.push_back(t0);
    vertexData.push_back(nodeIndexF);

    vertexData.push_back(x1);
    vertexData.push_back(y0);
    vertexData.push_back(depthF);
    vertexData.push_back(s1);
    vertexData.push_back(t0);
    vertexData.push_back(nodeIndexF);

    vertexData.push_back(x0);
    vertexData.push_back(y1);
    vertexData.push_back(depthF);
    vertexData.push_back(s0);
    vertexData.push_back(t1);
    vertexData.push_back(nodeIndexF);

    // Second triangle (CCW winding)
    vertexData.push_back(x1);
    vertexData.push_back(y0);
    vertexData.push_back(depthF);
    vertexData.push_back(s1);
    vertexData.push_back(t0);
    vertexData.push_back(nodeIndexF);

    vertexData.push_back(x1);
    vertexData.push_back(y1);
    vertexData.push_back(depthF);
    vertexData.push_back(s1);
    vertexData.push_back(t1);
    vertexData.push_back(nodeIndexF);

    vertexData.push_back(x0);
    vertexData.push_back(y1);
    vertexData.push_back(depthF);
    vertexData.push_back(s0);
    vertexData.push_back(t1);
    vertexData.push_back(nodeIndexF);

    currentX += glyph.advance * scale;
    charCount++;
  }

  return TextVertexResult(currentX, charCount);
}

std::vector<double> TextLayout::CalculateTextWidthBatch(
    const std::vector<std::string>& texts,
    double fontSize,
    const std::unordered_map<int, GlyphMetrics>& glyphMap) {
  std::vector<double> widths;
  widths.reserve(texts.size());

  for (const auto& text : texts) {
    widths.push_back(CalculateTextWidth(text, fontSize, glyphMap));
  }

  return widths;
}

std::vector<Vec2d> TextLayout::CalculatePortPositions(
    const std::vector<std::string>& inputPins,
    const std::vector<std::string>& outputPins,
    double fontSize,
    double portSpacing,
    const Vec2d& nodePosition,
    const Vec2d& nodeSize,
    const std::unordered_map<int, GlyphMetrics>& glyphMap,
    double marginH,
    double titleHeight) {
  std::vector<Vec2d> positions;
  positions.reserve(inputPins.size() + outputPins.size());

  // Port line height
  double portLineHeight = fontSize * 1.2 + portSpacing;
  double portNameHeight = fontSize * 1.2;

  // Starting Y position below title
  double portStartY = nodePosition[1] + titleHeight + marginH;

  // Input pin positions (left side of node)
  for (size_t i = 0; i < inputPins.size(); ++i) {
    double x = nodePosition[0] + marginH;
    double y = portStartY + static_cast<double>(i) * portLineHeight + portNameHeight * 0.5;
    positions.emplace_back(x, y);
  }

  // Output pin positions (right side of node)
  // Start at same Y as inputs (unless inputs push them down)
  double outputStartY = portStartY + static_cast<double>(inputPins.size()) * portLineHeight;

  for (size_t i = 0; i < outputPins.size(); ++i) {
    // Calculate text width to right-align
    double textWidth = CalculateTextWidth(outputPins[i], fontSize, glyphMap);
    double x = nodePosition[0] + nodeSize[0] - marginH - textWidth;
    double y = outputStartY + static_cast<double>(i) * portLineHeight + portNameHeight * 0.5;
    positions.emplace_back(x, y);
  }

  return positions;
}

} // namespace noodles
