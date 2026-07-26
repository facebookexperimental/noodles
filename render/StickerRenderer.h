// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_STICKER_RENDERER_H
#define NOODLES_RENDER_STICKER_RENDERER_H

#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/api.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <vector>

namespace noodles {

/// Per-backdrop render inputs. This is the C++-owned view of a group sticker:
/// the Python `GroupSticker` (USD-backed) is projected onto this plain struct
/// so all backdrop vertex generation lives in C++, not Python/Qt. (Distinct
/// from core::StickerData, the GraphModel backdrop record.)
struct NOODLES_API StickerRenderInput {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  // Fill color and alpha, 0-255.
  int r = 0;
  int g = 0;
  int b = 0;
  int a = 255;
  float innerStroke = 0.0f;
  // Stacking order among backdrops (higher = drawn in front, still behind nodes).
  int priority = 0;
  bool selected = false;
};

/// Frontmost backdrop depth. The whole backdrop band is kept strictly negative
/// so backdrops always render behind nodes (node quads live at z >= 0).
inline constexpr float kStickerDepthFront = -0.05f;
/// Depth step between adjacent-priority backdrops.
inline constexpr float kStickerDepthStep = 0.001f;
/// Selection highlight (bright yellow) blended into the fill of a picked
/// backdrop. Matches the node selection stroke color.
inline constexpr std::uint8_t kStickerSelectionR = 255;
inline constexpr std::uint8_t kStickerSelectionG = 255;
inline constexpr std::uint8_t kStickerSelectionB = 30;
inline constexpr std::uint8_t kStickerSelectionA = 255;
/// Partial blend weight so the backdrop's own color stays recognizable.
inline constexpr float kStickerSelectionBlend = 0.5f;

class NOODLES_API StickerRenderer {
 public:
  StickerRenderer();
  ~StickerRenderer();

  StickerRenderer(const StickerRenderer&) = delete;
  StickerRenderer& operator=(const StickerRenderer&) = delete;
  StickerRenderer(StickerRenderer&&) = delete;
  StickerRenderer& operator=(StickerRenderer&&) = delete;

  void initialize(ShaderLibrary* shaders);

  /// Build the GPU vertices for a set of backdrops: ordered back-to-front by
  /// priority into a strictly-negative depth band, two triangles per backdrop,
  /// with the selection highlight baked into selected backdrops. Pure (no GL),
  /// so it is unit-testable without a GL context.
  static std::vector<NodeVertex> buildStickerVertices(
      const std::vector<StickerRenderInput>& stickers);

  /// Generate vertices from backdrop data and draw them. This is the entry
  /// point used by the interactive editor.
  void renderStickerData(
      const std::vector<StickerRenderInput>& stickers,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& strokeColor);

  /// Draw pre-generated vertices. Retained for the (currently dormant)
  /// GraphRenderer path that supplies its own vertex buffer.
  void renderStickers(
      const std::vector<NodeVertex>& vertices,
      const float* projection4x4,
      float cornerRadius,
      const std::array<float, 4>& strokeColor);

  void markDirty();
  void cleanup();

 private:
  ShaderLibrary* shaders_ = nullptr;
  GLuint vbo_ = 0;
  GLuint vao_ = 0;
  bool initialized_ = false;
};

} // namespace noodles

#endif // NOODLES_RENDER_STICKER_RENDERER_H
