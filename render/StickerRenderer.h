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
#include <vector>

namespace noodles {

class NOODLES_API StickerRenderer {
 public:
  StickerRenderer();
  ~StickerRenderer();

  StickerRenderer(const StickerRenderer&) = delete;
  StickerRenderer& operator=(const StickerRenderer&) = delete;
  StickerRenderer(StickerRenderer&&) = delete;
  StickerRenderer& operator=(StickerRenderer&&) = delete;

  void initialize(ShaderLibrary* shaders);

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
