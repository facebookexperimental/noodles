// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_PROFILER_H
#define NOODLES_RENDER_PROFILER_H

#include "core/api.h"

#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

struct NOODLES_API ProfileStats {
  std::deque<double> samples;
  double total = 0.0;
  int count = 0;
  double minVal = 1e30;
  double maxVal = 0.0;
  size_t maxSamples = 60;

  void record(double elapsedMs);
  double average() const;
};

class NOODLES_API ProfileSection {
 public:
  ProfileSection(class RenderProfiler* profiler, std::string name);
  ~ProfileSection();

  ProfileSection(const ProfileSection&) = delete;
  ProfileSection& operator=(const ProfileSection&) = delete;
  ProfileSection(ProfileSection&&) = delete;
  ProfileSection& operator=(ProfileSection&&) = delete;

 private:
  class RenderProfiler* profiler_;
  std::string name_;
  std::chrono::high_resolution_clock::time_point start_;
};

/// No-op stand-in for ProfileSection when profiling is disabled,
/// allowing callers to use the same code path without branching.
class NOODLES_API DummyContext {
 public:
  DummyContext() = default;
};

class NOODLES_API RenderProfiler {
 public:
  explicit RenderProfiler(int sampleSize = 60);

  void startFrame();
  void endFrame();

  ProfileSection timeSection(const std::string& name);
  void recordTime(const std::string& name, double elapsedMs);

  std::string getStats() const;
  void printStats() const;
  void reset();

  bool enabled = false;

 private:
  int sampleSize_;
  std::unordered_map<std::string, ProfileStats> timings_;
  std::deque<double> frameTimes_;
  std::chrono::high_resolution_clock::time_point lastFrameStart_;
  bool frameStarted_ = false;
};

} // namespace noodles

#endif // NOODLES_RENDER_PROFILER_H
