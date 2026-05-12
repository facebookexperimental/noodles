// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_RENDER_CONFIG_H
#define NOODLES_CORE_RENDER_CONFIG_H

#include "core/api.h"
#include "core/pragmas.h"

#include <array>
#include <string>
#include <unordered_map>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API RenderConfig {
  double globalNodeScale = 2.0;
  double nodeTitleFontSize = 24.0;
  double nodePinFontSize = 18.0;
  double nodePinTypeFontSize = 14.0;
  double nodeMarginH = 16.0;
  double nodeMarginV = 18.0;
  double nodePortSpacing = 1.0;
  double nodePortWidth = 16.0;
  double nodeCornerRadius = 10.0;
  double selectedNodeStrokeWidth = 4.0;
  std::array<float, 4> selectedNodeStrokeColor = {1.0f, 1.0f, 0.117f, 1.0f};
  int nodeBgHigh = 50;
  int nodeBgLow = 40;
  int nodeBgAlpha = 248;
  double nodeShadowFactor = 0.6;
  double nodeTypeSaturation = 0.6;
  double nodeTypeBrightness = 0.6;
  double nodePortRadius = 8.0;
  double nodeFontSize = 14.0;
  std::string nodeRendererType = "default";
  bool isGraffiStyle = false;
  std::array<float, 4> backgroundClearColor = {0.18f, 0.18f, 0.22f, 1.0f};
  double linkLineWidth = 10.0;
  double linkSampleRate = 6.0;
  double linkEdgeDimmingStart = 0.7;
  double linkEdgeDimmingEnd = 0.5;
  double linkCutoffAlpha = 0.93;

  double get(const std::string& key, double defaultValue) const;
  int getInt(const std::string& key, int defaultValue) const;
  std::string getString(const std::string& key, const std::string& defaultValue) const;

 private:
  static std::string camelToSnake(const std::string& name);
  static const std::unordered_map<std::string, double RenderConfig::*>& doubleFields();
  static const std::unordered_map<std::string, int RenderConfig::*>& intFields();
  static const std::unordered_map<std::string, std::string RenderConfig::*>& stringFields();
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_CORE_RENDER_CONFIG_H
