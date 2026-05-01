// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_NODE_DATA_H
#define NOODLES_CORE_NODE_DATA_H

#include "core/Math.h"
#include "core/api.h"
#include "core/pragmas.h"

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace noodles {

struct FontMetrics;
struct RenderConfig;

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API NodeData {
  std::string id;
  std::string name;
  std::string type;
  Vec2d position{0.0, 0.0};
  Vec2d size{200.0, 100.0};
  std::vector<std::string> inputPins;
  std::vector<std::string> outputPins;
  std::unordered_map<std::string, std::string> inputPinTypes;
  std::unordered_map<std::string, std::string> outputPinTypes;
  std::vector<int> inputLinks;
  std::vector<int> outputLinks;
  std::vector<int> inputRowKinds; // parallel to inputPins: 0=normal 1=folded 2=unfolded 3=child
  std::vector<int> outputRowKinds; // parallel to outputPins: 0=normal 1=folded 2=unfolded 3=child
  bool selected = false;
  bool titleCollapsed = false; // true = all rows hidden, single aggregate port
  std::string uiStyle = "default";

  int textStartIndex = 0;
  int textNumChars = 0;

  int zOrder = 0;

  float displayColorR = -1.0f;
  float displayColorG = -1.0f;
  float displayColorB = -1.0f;
};

struct NOODLES_API LinkData {
  static constexpr double DANGLING_LINK_LENGTH = 50.0;

  std::string sourceNodeId;
  std::string sourcePort;
  std::string targetNodeId;
  std::string targetPort;
  Vec2d start{0.0, 0.0};
  Vec2d end{0.0, 0.0};
  bool selected = false;
  bool hovered = false;
  bool isDangling = false;
  std::string danglingDirection;
  std::array<float, 4> color = {0.0f, 0.0f, 0.0f, 0.0f};
  bool highlighted = false;
  std::array<float, 4> highlightColor = {0.0f, 0.0f, 0.0f, 0.0f};
  bool hasColor = false;
};

struct NOODLES_API StickerData {
  std::string id;
  std::string label;
  Vec2d position{0.0, 0.0};
  Vec2d size{400.0, 300.0};
  float r = 0.2f, g = 0.2f, b = 0.3f, a = 0.5f;
  bool selected = false;
  std::vector<std::string> nodeIds;
};

struct NOODLES_API FontMetrics {
  double ascender = 0.0;
  double descender = 0.0;
  double lineHeight = 0.0;
};

using TextWidthCallback = std::function<double(const std::string& text, double fontSize)>;

class NOODLES_API GraphModel {
 public:
  std::unordered_map<std::string, NodeData> nodes;
  std::vector<LinkData> links;
  std::vector<StickerData> stickers;

  void buildConnectionCache();
  std::unordered_set<std::string> getConnectedNodeIds(const std::string& nodeId) const;
  void clear();

  void markLinksChanged() {
    linksChanged_ = true;
  }
  bool isLinksChanged() const {
    return linksChanged_;
  }

  void calculateNodeSize(
      NodeData& node,
      const TextWidthCallback& calculateTextWidth,
      const FontMetrics& fontMetrics,
      const RenderConfig* config = nullptr);

 private:
  void rebuildConnectionCache() const;
  mutable std::unordered_map<std::string, std::unordered_set<std::string>> connectionCache_;
  mutable bool linksChanged_ = true;
};

NOODLES_PRAGMA_POP

} // namespace noodles
#endif // NOODLES_CORE_NODE_DATA_H
