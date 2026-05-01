// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_FONT_ATLAS_H
#define NOODLES_RENDER_FONT_ATLAS_H

#include "core/Glyph.h"
#include "core/api.h"

#include <GL/glew.h>

#include <string>
#include <unordered_map>

namespace noodles {

class NOODLES_API FontAtlas {
 public:
  FontAtlas() = default;
  ~FontAtlas();

  FontAtlas(const FontAtlas&) = delete;
  FontAtlas& operator=(const FontAtlas&) = delete;
  FontAtlas(FontAtlas&& other) noexcept;
  FontAtlas& operator=(FontAtlas&& other) noexcept;

  void initialize(const std::string& atlasPath, const std::string& jsonPath);
  void bindTexture(int textureUnit = 0) const;
  void unbindTexture() const;
  void cleanup();

  GLuint textureId() const {
    return textureId_;
  }
  const std::unordered_map<int, Glyph>& glyphs() const {
    return glyphs_;
  }
  double pxRange() const {
    return pxRange_;
  }
  double lineHeight() const {
    return lineHeight_;
  }
  double ascender() const {
    return ascender_;
  }
  double descender() const {
    return descender_;
  }

 private:
  void loadAtlas(const std::string& atlasPath);
  void loadMetrics(const std::string& jsonPath);

  GLuint textureId_ = 0;
  std::unordered_map<int, Glyph> glyphs_;
  double pxRange_ = 4.0;
  double lineHeight_ = 1.2;
  double ascender_ = 0.8;
  double descender_ = -0.2;
};

} // namespace noodles

#endif // NOODLES_RENDER_FONT_ATLAS_H
