// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_NODE_TRANSFORM_FRAME_H
#define NOODLES_RENDER_NODE_TRANSFORM_FRAME_H

#include "core/Math.h"
#include "core/NodeData.h"
#include "core/api.h"
#include "render/ShaderLibrary.h"

#include <GL/glew.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace noodles {

/// Per-node move offsets (RG32F buffer texture) shared by the text and node-quad
/// shaders. Geometry is baked at a node's base position; drags only update the
/// offset here, so nothing regenerates.
///
/// Buffer texture (not 1D) so capacity can grow past GL_MAX_TEXTURE_SIZE.
/// Slots are stable for a node's lifetime — the text and node-quad paths
/// generate at different times against a rehashing map, so order-dependent
/// indices would desync the two layers' baked nodeIndex.
class NOODLES_API NodeTransformFrame {
 public:
  // Capacity doubles to fit the node count.
  static constexpr int kInitialCapacity = 1024;
  static constexpr int kMaxCapacity = 1 << 20;

  explicit NodeTransformFrame(int initialCapacityTexels = kInitialCapacity);
  ~NodeTransformFrame();

  NodeTransformFrame(const NodeTransformFrame&) = delete;
  NodeTransformFrame& operator=(const NodeTransformFrame&) = delete;
  NodeTransformFrame(NodeTransformFrame&&) = delete;
  NodeTransformFrame& operator=(NodeTransformFrame&&) = delete;

  // GL-free: assign slots to new nodes, prune deleted ones, grow capacity.
  void syncFromNodes(const std::unordered_map<std::string, NodeData>& nodes);
  // -1.0 if unknown or beyond capacity.
  float shaderIndex(const std::string& nodeId) const;
  Vec2d basePosition(const std::string& nodeId) const;
  // Accumulated live move offset (drag delta) for a node, {0,0} if none. The
  // node-quad / text shaders add this via the transform texture; CPU consumers
  // that compute absolute positions (e.g. the port-highlight producer) add it
  // themselves so their geometry follows a drag too.
  Vec2d offset(const std::string& nodeId) const;
  // The node's live top-left in world space: base + accumulated move offset when
  // the node has a transform slot, else `fallback` (the GraphModel snapshot
  // position). This is where the node's geometry actually draws on the GPU, so
  // viewport culling must test against it — never the snapshot, which is not
  // re-synced on a drag (shared by the text + icon culls).
  Vec2d liveOrigin(const std::string& nodeId, const Vec2d& fallback) const;
  void translate(const std::string& nodeId, double dx, double dy);
  void reset();
  // Requires a GL context.
  void upload();
  void bindToShader(GLSLProgram* shader, int textureUnit);
  void cleanup();

  int capacity() const {
    return capacity_;
  }

  // Test-only.
  const std::unordered_map<std::string, int>& idToIndexForTest() const {
    return idToIndex_;
  }
  std::pair<float, float> offsetForTest(const std::string& nodeId) const {
    auto it = transforms_.find(nodeId);
    return it == transforms_.end() ? std::pair<float, float>{0.0f, 0.0f} : it->second;
  }
  // GL buffer object handle (GL_TEXTURE_BUFFER) — exposed for tests that
  // verify upload correctness via glGetBufferSubData. 0 if upload() has not
  // run successfully (e.g., glTexBuffer was unresolved on OSMesa).
  GLuint bufferForTest() const {
    return buffer_;
  }

 private:
  void ensureBuffer();
  int allocateSlot();
  void growCapacity(int neededTexels);

  GLuint texture_ = 0;
  GLuint buffer_ = 0;
  int capacity_;
  int glBufferCapacity_ = 0; // texels actually allocated on the GPU
  bool dirty_ = true;
  int nextSlot_ = 0;
  std::vector<int> freeSlots_;
  std::unordered_map<std::string, int> idToIndex_;
  std::unordered_map<std::string, std::pair<float, float>> transforms_;
  std::unordered_map<std::string, std::pair<double, double>> basePositions_;
  // Reused so a drag doesn't reallocate every frame.
  std::vector<float> uploadStaging_;
};

} // namespace noodles

#endif // NOODLES_RENDER_NODE_TRANSFORM_FRAME_H
