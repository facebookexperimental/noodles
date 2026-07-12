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
  // Node chrome dimensions, in graph world-space units (1 unit = 1 px at the
  // default 1.0 camera zoom).
  double nodeTitleFontSize = 48.0;
  double nodePinFontSize = 36.0;
  double nodePinTypeFontSize = 26.0;
  double nodeMarginH = 30.0;
  double nodeMarginV = 20.0;
  double nodePortSpacing = 12.0;
  double nodePortWidth = 40.0;
  double nodeCornerRadius = 14.0;
  double selectedNodeStrokeWidth = 10.0;
  std::array<float, 4> selectedNodeStrokeColor = {1.0f, 1.0f, 0.117f, 1.0f};
  int nodeBgHigh = 90;
  int nodeBgLow = 86;
  int nodeBgAlpha = 245;
  double nodeShadowFactor = 0.9;
  double nodeTypeSaturation = 0.35;
  double nodeTypeBrightness = 0.5;
  double nodePortRadius = 8.0;
  double nodeFontSize = 14.0;
  // Port-highlight appearance (mirror of the usd_noodles `portRingThickness` /
  // `port*Color` settings, cached in graphView). Used by the C++ port-highlight
  // producer; Python pushes the live setting values onto these before rendering.
  double nodePortRingThickness = 2.0;
  std::array<float, 4> portConnectedFillColor = {0.7f, 0.8f, 0.0f, 1.0f};
  std::array<float, 4> portConnectedRingColor = {1.0f, 1.0f, 1.0f, 0.0f};
  std::array<float, 4> portDisconnectedFillColor = {0.235f, 0.235f, 0.235f, 1.0f};
  std::array<float, 4> portDisconnectedRingColor = {0.7f, 0.8f, 0.0f, 1.0f};
  std::array<float, 4> portHoverColor = {0.4f, 0.7f, 1.0f, 1.0f};
  static constexpr double kSchemaTypeFontRatio = 0.65;
  // Z-depth step between consecutive z-order levels (mirror of
  // usd_noodles/constants.py DEPTH_STEP). Node backgrounds sit at
  // ``zOrder * kDepthStep``; text/ports at ``+ kDepthStep * 0.5``.
  static constexpr double kDepthStep = 0.001;
  // Depth of a node's content (text/ports/icons): half a depth-step in front of
  // the node background (which sits at zOrder * kDepthStep), so content raises
  // with the node on selection. Mirrors usd_noodles/constants.py
  // node_content_depth().
  static constexpr double contentDepth(int zOrder) {
    return zOrder * kDepthStep + kDepthStep * 0.5;
  }
  // Depth of a node's background quad: the zOrder band itself, behind the
  // node's content. Mirrors graphView paintNodes (depth = zOrder * DEPTH_STEP).
  static constexpr double backgroundDepth(int zOrder) {
    return zOrder * kDepthStep;
  }
  // Pin-row icon sizing ratios (square icons scaled from the visible row
  // height; mirror of usd_noodles/constants.py).
  static constexpr double kRowIconSizeRatio = 0.64;
  static constexpr double kRelationshipRowIconSizeRatio = 0.72;
  static constexpr double kRowIconGapRatio = 0.25;
  std::string nodeRendererType = "default";
  bool isGraffiStyle = false;
  // When false, the "(type)" subtitle under each pin name is not laid out or
  // drawn, and pin rows shrink by the type-label height (calculateNodeSize zeros
  // portTypeHeight). Default true preserves the original appearance.
  bool showPinTypeLabels = true;
  std::array<float, 4> backgroundClearColor = {0.18f, 0.18f, 0.22f, 1.0f};
  double linkLineWidth = 12.0;
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
