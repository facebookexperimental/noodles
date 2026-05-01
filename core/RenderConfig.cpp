// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/RenderConfig.h"

#include <cctype>

namespace noodles {

std::string RenderConfig::camelToSnake(const std::string& name) {
  std::string result;
  for (size_t i = 0; i < name.size(); ++i) {
    if (std::isupper(name[i]) && i > 0) {
      result += '_';
    }
    result += static_cast<char>(std::tolower(name[i]));
  }
  return result;
}

const std::unordered_map<std::string, double RenderConfig::*>& RenderConfig::doubleFields() {
  static const std::unordered_map<std::string, double RenderConfig::*> fields = {
      {"global_node_scale", &RenderConfig::globalNodeScale},
      {"node_title_font_size", &RenderConfig::nodeTitleFontSize},
      {"node_pin_font_size", &RenderConfig::nodePinFontSize},
      {"node_pin_type_font_size", &RenderConfig::nodePinTypeFontSize},
      {"node_margin_h", &RenderConfig::nodeMarginH},
      {"node_margin_v", &RenderConfig::nodeMarginV},
      {"node_port_spacing", &RenderConfig::nodePortSpacing},
      {"node_port_width", &RenderConfig::nodePortWidth},
      {"node_corner_radius", &RenderConfig::nodeCornerRadius},
      {"selected_node_stroke_width", &RenderConfig::selectedNodeStrokeWidth},
      {"node_shadow_factor", &RenderConfig::nodeShadowFactor},
      {"node_type_saturation", &RenderConfig::nodeTypeSaturation},
      {"node_type_brightness", &RenderConfig::nodeTypeBrightness},
      {"node_port_radius", &RenderConfig::nodePortRadius},
      {"node_font_size", &RenderConfig::nodeFontSize},
      {"link_line_width", &RenderConfig::linkLineWidth},
      {"link_sample_rate", &RenderConfig::linkSampleRate},
      {"link_edge_dimming_start", &RenderConfig::linkEdgeDimmingStart},
      {"link_edge_dimming_end", &RenderConfig::linkEdgeDimmingEnd},
      {"link_cutoff_alpha", &RenderConfig::linkCutoffAlpha},
  };
  return fields;
}

const std::unordered_map<std::string, int RenderConfig::*>& RenderConfig::intFields() {
  static const std::unordered_map<std::string, int RenderConfig::*> fields = {
      {"node_bg_high", &RenderConfig::nodeBgHigh},
      {"node_bg_low", &RenderConfig::nodeBgLow},
      {"node_bg_alpha", &RenderConfig::nodeBgAlpha},
  };
  return fields;
}

const std::unordered_map<std::string, std::string RenderConfig::*>& RenderConfig::stringFields() {
  static const std::unordered_map<std::string, std::string RenderConfig::*> fields = {
      {"node_renderer_type", &RenderConfig::nodeRendererType},
  };
  return fields;
}

double RenderConfig::get(const std::string& key, double defaultValue) const {
  std::string snake = camelToSnake(key);
  auto it = doubleFields().find(snake);
  if (it != doubleFields().end()) {
    return this->*(it->second);
  }
  return defaultValue;
}

int RenderConfig::getInt(const std::string& key, int defaultValue) const {
  std::string snake = camelToSnake(key);
  auto it = intFields().find(snake);
  if (it != intFields().end()) {
    return this->*(it->second);
  }
  return defaultValue;
}

std::string RenderConfig::getString(const std::string& key, const std::string& defaultValue) const {
  std::string snake = camelToSnake(key);
  auto it = stringFields().find(snake);
  if (it != stringFields().end()) {
    return this->*(it->second);
  }
  return defaultValue;
}

} // namespace noodles
