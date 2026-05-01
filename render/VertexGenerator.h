// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_VERTEX_GENERATOR_H
#define NOODLES_RENDER_VERTEX_GENERATOR_H

#include <cstdint>
#include <string>
#include <vector>
#include "core/Math.h"
#include "core/NodeVertex.h"
#include "core/api.h"

namespace noodles {

struct NOODLES_API PortInfo {
  Vec2d position{0.0, 0.0};
  uint8_t r = 128, g = 128, b = 128, a = 255;
};

struct NOODLES_API DefaultNodeColors {
  int bgHigh, bgLow, bgAlpha;
  int bgHighShadow, bgLowShadow;
  int selectedBgHigh, selectedBgLow;
  int selectedBgHighShadow, selectedBgLowShadow;
  int titleR, titleG, titleB;
  int titleRShadow, titleGShadow, titleBShadow;
  int selectedTitleR, selectedTitleG, selectedTitleB;
  int selectedTitleRShadow, selectedTitleGShadow, selectedTitleBShadow;
};

struct NOODLES_API GraffiNodeColors {
  int bodyR, bodyG, bodyB, bodyAlpha;
  int headerR, headerG, headerB, headerAlpha;
  int selectedBodyR, selectedBodyG, selectedBodyB;
  int selectedHeaderR, selectedHeaderG, selectedHeaderB;
};

class NOODLES_API VertexGenerator {
 public:
  static std::vector<NodeVertex> generateDefaultNodeVertices(
      const Vec2d& nodePos,
      const Vec2d& nodeSize,
      float depth,
      float titleHeight,
      const DefaultNodeColors& colors,
      float innerStroke,
      float selectedFlag,
      int pinCount = 0,
      float portLineHeight = 0.0f,
      float portStartY = 0.0f,
      const std::vector<int>& rowKinds = {});

  static Vec4f hsvToRgb(float h, float s, float v, float a = 1.0f);

  static std::vector<NodeVertex> generateGraffiNodeVertices(
      const Vec2d& nodePos,
      const Vec2d& nodeSize,
      float depth,
      float titleHeight,
      const GraffiNodeColors& colors,
      const std::vector<PortInfo>& inputPorts,
      const std::vector<PortInfo>& outputPorts,
      float portRadius,
      float bodyStroke,
      float selectedFlag);
};

} // namespace noodles

#endif // NOODLES_RENDER_VERTEX_GENERATOR_H
