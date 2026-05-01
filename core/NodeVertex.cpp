// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/NodeVertex.h"
#include <cstring>
#include <GL/glew.h>

namespace noodles {

NodeVertex::NodeVertex(
    float x,
    float y,
    float z,
    float u,
    float v,
    float w,
    float h,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a,
    float innerStroke,
    uint8_t sr,
    uint8_t sg,
    uint8_t sb,
    uint8_t sa,
    float selected)
    : x(x),
      y(y),
      z(z),
      u(u),
      v(v),
      w(w),
      h(h),
      r(r),
      g(g),
      b(b),
      a(a),
      innerStroke(innerStroke),
      sr(sr),
      sg(sg),
      sb(sb),
      sa(sa),
      selected(selected) {}

std::vector<uint8_t> NodeVertex::pack() const {
  // Pack vertex data matching Python struct format 'fffffffBBBBfBBBBf'
  // 7 floats, 4 bytes, 1 float, 4 bytes, 1 float = 44 bytes total
  std::vector<uint8_t> data;
  data.reserve(44);

  // Position (x, y, z)
  const auto* xPtr = reinterpret_cast<const uint8_t*>(&x);
  data.insert(data.end(), xPtr, xPtr + sizeof(float));
  const auto* yPtr = reinterpret_cast<const uint8_t*>(&y);
  data.insert(data.end(), yPtr, yPtr + sizeof(float));
  const auto* zPtr = reinterpret_cast<const uint8_t*>(&z);
  data.insert(data.end(), zPtr, zPtr + sizeof(float));

  // UV (u, v)
  const auto* uPtr = reinterpret_cast<const uint8_t*>(&u);
  data.insert(data.end(), uPtr, uPtr + sizeof(float));
  const auto* vPtr = reinterpret_cast<const uint8_t*>(&v);
  data.insert(data.end(), vPtr, vPtr + sizeof(float));

  // Size (w, h)
  const auto* wPtr = reinterpret_cast<const uint8_t*>(&w);
  data.insert(data.end(), wPtr, wPtr + sizeof(float));
  const auto* hPtr = reinterpret_cast<const uint8_t*>(&h);
  data.insert(data.end(), hPtr, hPtr + sizeof(float));

  // Color (r, g, b, a)
  data.push_back(r);
  data.push_back(g);
  data.push_back(b);
  data.push_back(a);

  // Inner stroke
  const auto* innerStrokePtr = reinterpret_cast<const uint8_t*>(&innerStroke);
  data.insert(data.end(), innerStrokePtr, innerStrokePtr + sizeof(float));

  // Selected color (sr, sg, sb, sa)
  data.push_back(sr);
  data.push_back(sg);
  data.push_back(sb);
  data.push_back(sa);

  // Selected flag
  const auto* selectedPtr = reinterpret_cast<const uint8_t*>(&selected);
  data.insert(data.end(), selectedPtr, selectedPtr + sizeof(float));

  return data;
}

std::vector<uint8_t> NodeVertex::packBatch(const std::vector<NodeVertex>& vertices) {
  if (vertices.empty()) {
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> data;
  data.reserve(vertices.size() * 44);

  for (const auto& vertex : vertices) {
    auto packedVertex = vertex.pack();
    data.insert(data.end(), packedVertex.begin(), packedVertex.end());
  }

  return data;
}

size_t NodeVertex::size() {
  return 44;
}

void NodeVertex::setupVertexAttribs() {
  GLsizei stride = 44;
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)20);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)28);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)32);
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)36);
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (void*)40);
}

} // namespace noodles
