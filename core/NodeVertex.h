// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_NODE_VERTEX_H
#define NOODLES_CORE_NODE_VERTEX_H

#include <cstdint>
#include <vector>
#include "core/api.h"

namespace noodles {

/// Binary-compatible vertex structure for GPU upload
/// Layout: position(3f), uv(2f), size(2f), color(4B), innerStroke(1f), selectedColor(4B),
/// selected(1f)
struct NOODLES_API NodeVertex {
  float x, y, z;
  float u, v;
  float w, h;
  uint8_t r, g, b, a;

  // Inner stroke flag
  float innerStroke;

  // Selected color (sr, sg, sb, sa)
  uint8_t sr, sg, sb, sa;

  // Selected flag (0.0 = unselected, 1.0 = selected)
  float selected;

  NodeVertex(
      float x = 0,
      float y = 0,
      float z = 0,
      float u = 0,
      float v = 0,
      float w = 0,
      float h = 0,
      uint8_t r = 0,
      uint8_t g = 0,
      uint8_t b = 0,
      uint8_t a = 255,
      float innerStroke = 0,
      uint8_t sr = 0,
      uint8_t sg = 0,
      uint8_t sb = 0,
      uint8_t sa = 255,
      float selected = 0.0f);

  /// Pack to binary format for VBO upload (matches Python struct format 'fffffffBBBBfBBBBf')
  std::vector<uint8_t> pack() const;

  /// Batch pack multiple vertices efficiently
  static std::vector<uint8_t> packBatch(const std::vector<NodeVertex>& vertices);

  static size_t size();

  /// Configure OpenGL vertex attrib pointers for the NodeVertex layout.
  /// Must be called with a VAO and VBO already bound.
  static void setupVertexAttribs();
};

} // namespace noodles

#endif // NOODLES_CORE_NODE_VERTEX_H
