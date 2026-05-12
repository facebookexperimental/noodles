// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_TEXT_LAYOUT_H
#define NOODLES_RENDER_TEXT_LAYOUT_H

#include "core/Math.h"
#include "core/api.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

/// \class GlyphMetrics
/// \brief Font glyph metrics for text rendering calculations.
///
/// Stores the essential metrics for a single glyph from a font atlas,
/// including advance width and UV coordinates for texture lookup.
/// Corresponds to the Glyph class in models.py.
struct NOODLES_API GlyphMetrics {
  /// Unicode code point for this glyph
  int unicode;

  /// Horizontal advance (spacing to next character)
  double advance;

  /// Plane bounds in em units (used for vertex positioning)
  /// Min = (left, top), Max = (right, bottom)
  Range2d planeBounds;

  /// Atlas UV bounds (normalized texture coordinates)
  /// Min = (s0, t0), Max = (s1, t1)
  Range2d atlasBounds;

  GlyphMetrics() : unicode(0), advance(0.0), planeBounds(), atlasBounds() {}

  GlyphMetrics(int unicodeVal, double adv, const Range2d& plane, const Range2d& atlas)
      : unicode(unicodeVal), advance(adv), planeBounds(plane), atlasBounds(atlas) {}
};

/// \class TextVertexResult
/// \brief Result of text vertex generation, containing vertex data and cursor position.
struct NOODLES_API TextVertexResult {
  /// Final cursor X position after generating all vertices
  double cursorX;

  /// Number of characters successfully rendered
  int charCount;

  TextVertexResult() : cursorX(0.0), charCount(0) {}
  TextVertexResult(double x, int count) : cursorX(x), charCount(count) {}
};

/// \class TextLayout
/// \brief High-performance text layout calculator for node graph rendering.
///
/// Provides optimized text width calculation and vertex generation for
/// MSDF (Multi-channel Signed Distance Field) text rendering.
/// This C++ implementation provides 2-3x speedup over the Python textRenderer.py.
///
/// Key features:
/// - Cached text width calculations
/// - Batch vertex generation for reduced Python/C++ crossing overhead
/// - Direct memory layout matching GPU vertex format
class NOODLES_API TextLayout {
 public:
  /// Calculate text width in pixels/units.
  /// Uses glyph advance values to compute total width.
  ///
  /// \param text The text string to measure
  /// \param fontSize The font size (scale factor)
  /// \param glyphMap Map of unicode -> GlyphMetrics
  /// \return Width of the text in the same units as fontSize
  static double CalculateTextWidth(
      const std::string& text,
      double fontSize,
      const std::unordered_map<int, GlyphMetrics>& glyphMap);

  /// Generate text vertices for GPU rendering.
  /// Creates 6 vertices per character (2 triangles per quad).
  ///
  /// Vertex format: x, y, z, s, t, nodeIndex (6 floats per vertex)
  /// - x, y, z: Position in world coordinates
  /// - s, t: Texture UV coordinates
  /// - nodeIndex: Index for GPU-side transform lookup
  ///
  /// \param text The text string to render
  /// \param cursorX Starting X position
  /// \param cursorY Starting Y position (baseline)
  /// \param depth Z-depth for rendering order
  /// \param scale Font scale (font size)
  /// \param glyphMap Map of unicode -> GlyphMetrics
  /// \param ascender Font ascender value (from font atlas metrics)
  /// \param nodeIndex Node index for GPU transform lookup (default 0.0)
  /// \param vertexData Output vector to append vertex data to
  /// \return TextVertexResult with final cursor position and character count
  static TextVertexResult GenerateTextVertices(
      const std::string& text,
      double cursorX,
      double cursorY,
      double depth,
      double scale,
      const std::unordered_map<int, GlyphMetrics>& glyphMap,
      double ascender,
      double nodeIndex,
      std::vector<float>& vertexData);

  /// Batch calculate text widths for multiple strings.
  ///
  /// \param texts Vector of text strings to measure
  /// \param fontSize The font size (scale factor)
  /// \param glyphMap Map of unicode -> GlyphMetrics
  /// \return Vector of widths in the same order as input texts
  static std::vector<double> CalculateTextWidthBatch(
      const std::vector<std::string>& texts,
      double fontSize,
      const std::unordered_map<int, GlyphMetrics>& glyphMap);

  /// Calculate port positions for a node (batch operation).
  ///
  /// This method is specifically optimized for the node graph use case where
  /// we need to calculate positions for multiple port labels at once.
  ///
  /// \param inputPins Vector of input pin names
  /// \param outputPins Vector of output pin names
  /// \param fontSize Font size for the port labels
  /// \param portSpacing Vertical spacing between ports
  /// \param nodePosition Node's top-left position
  /// \param nodeSize Node's width and height
  /// \param glyphMap Map of unicode -> GlyphMetrics
  /// \param marginH Horizontal margin
  /// \param titleHeight Height of the title area
  /// \return Vector of port center positions (input ports first, then output ports)
  static std::vector<Vec2d> CalculatePortPositions(
      const std::vector<std::string>& inputPins,
      const std::vector<std::string>& outputPins,
      double fontSize,
      double portSpacing,
      const Vec2d& nodePosition,
      const Vec2d& nodeSize,
      const std::unordered_map<int, GlyphMetrics>& glyphMap,
      double marginH,
      double titleHeight);

  /// Calculate port positions using explicit authored row slots.
  ///
  /// \param inputRowSlots Visible row index for each input pin
  /// \param outputRowSlots Visible row index for each output pin
  static std::vector<Vec2d> CalculatePortPositions(
      const std::vector<std::string>& inputPins,
      const std::vector<std::string>& outputPins,
      double fontSize,
      double portSpacing,
      const Vec2d& nodePosition,
      const Vec2d& nodeSize,
      const std::unordered_map<int, GlyphMetrics>& glyphMap,
      double marginH,
      double titleHeight,
      const std::vector<int>& inputRowSlots,
      const std::vector<int>& outputRowSlots);

 private:
  /// Internal helper for UTF-8 to codepoint conversion
  static int _Utf8ToCodepoint(const char* str, int* bytesConsumed);
};

} // namespace noodles

#endif // NOODLES_RENDER_TEXT_LAYOUT_H
