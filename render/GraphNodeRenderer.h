// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_GRAPH_NODE_RENDERER_H
#define NOODLES_RENDER_GRAPH_NODE_RENDERER_H

#include "core/NodeData.h"
#include "core/NodeVertex.h"
#include "core/RenderConfig.h"
#include "core/api.h"
#include "render/FontAtlas.h"
#include "render/VertexGenerator.h"

#include <array>
#include <string>
#include <tuple>
#include <vector>

namespace noodles {

class NOODLES_API GraphNodeRenderer {
 public:
  explicit GraphNodeRenderer(const RenderConfig& config = RenderConfig());
  virtual ~GraphNodeRenderer() = default;

  virtual std::vector<NodeVertex>
  generateVertices(NodeData& node, float depth, const FontAtlas& fontAtlas) = 0;

  virtual double getGlobalScale() const;
  virtual double getCornerRadius(const NodeData& node) const;
  virtual double getSelectedStrokeWidth() const;
  virtual double getUnselectedStrokeWidth() const;
  virtual std::array<float, 4> getStrokeColor(const NodeData& node) const;
  virtual bool getOutputPortsTopToBottom() const;
  virtual double getPortFontSize() const;
  virtual double getPortMarginH() const;
  virtual double getPortMarginV() const;
  virtual double getPortSpacing() const;
  virtual double getPortWidth() const;
  virtual double getTitleFontSize() const;
  virtual double getPortTypeFontSize() const;
  virtual bool isGraffiStyle() const {
    return false;
  }

  // Port hit testing
  struct PortHitResult {
    std::string portName;
    bool isOutput = false;
    bool found = false;
  };

  PortHitResult getPortAtPoint(
      const NodeData& node,
      const Vec2d& worldPos,
      const FontAtlas& fontAtlas,
      double hitRadiusMultiplier = 1.5) const;

  PortHitResult getGroupHeaderAtPoint(
      const NodeData& node,
      const Vec2d& worldPos,
      const FontAtlas& fontAtlas) const;

  Vec2d getPortPosition(
      const NodeData& node,
      const std::string& portName,
      bool isOutput,
      const FontAtlas& fontAtlas) const;

  struct PortHitArea {
    double minX, minY, maxX, maxY;
  };

  PortHitArea getPortHitArea(
      const NodeData& node,
      const std::string& portName,
      bool isOutput,
      const FontAtlas& fontAtlas) const;

 protected:
  RenderConfig config_;

 private:
  static bool pointInRect(const Vec2d& point, const PortHitArea& rect);
};

class NOODLES_API DefaultNodeRenderer : public GraphNodeRenderer {
 public:
  explicit DefaultNodeRenderer(const RenderConfig& config = RenderConfig());

  std::vector<NodeVertex> generateVertices(NodeData& node, float depth, const FontAtlas& fontAtlas)
      override;
};

class NOODLES_API GraffiNodeRenderer : public GraphNodeRenderer {
 public:
  explicit GraffiNodeRenderer(const RenderConfig& config = RenderConfig());

  std::vector<NodeVertex> generateVertices(NodeData& node, float depth, const FontAtlas& fontAtlas)
      override;

  std::array<float, 4> getStrokeColor(const NodeData& node) const override;
  bool getOutputPortsTopToBottom() const override;
  double getUnselectedStrokeWidth() const override;
  double getPortFontSize() const override;
  double getPortMarginH() const override;
  double getPortMarginV() const override;
  double getPortSpacing() const override;
  double getTitleFontSize() const override;
  double getPortTypeFontSize() const override;
  bool isGraffiStyle() const override {
    return true;
  }
};

} // namespace noodles

#endif // NOODLES_RENDER_GRAPH_NODE_RENDERER_H
