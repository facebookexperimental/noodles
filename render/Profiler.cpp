// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/Profiler.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <sstream>

namespace noodles {

void ProfileStats::record(double elapsedMs) {
  samples.push_back(elapsedMs);
  if (samples.size() > maxSamples) {
    samples.pop_front();
  }
  total += elapsedMs;
  count++;
  minVal = std::min(minVal, elapsedMs);
  maxVal = std::max(maxVal, elapsedMs);
}

double ProfileStats::average() const {
  if (samples.empty()) {
    return 0.0;
  }
  double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  return sum / static_cast<double>(samples.size());
}

ProfileSection::ProfileSection(RenderProfiler* profiler, std::string name)
    : profiler_(profiler),
      name_(std::move(name)),
      start_(std::chrono::high_resolution_clock::now()) {}

ProfileSection::~ProfileSection() {
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed = std::chrono::duration<double, std::milli>(end - start_).count();
  profiler_->recordTime(name_, elapsed);
}

RenderProfiler::RenderProfiler(int sampleSize) : sampleSize_(sampleSize) {}

void RenderProfiler::startFrame() {
  if (!enabled) {
    return;
  }
  lastFrameStart_ = std::chrono::high_resolution_clock::now();
  frameStarted_ = true;
}

void RenderProfiler::endFrame() {
  if (!enabled || !frameStarted_) {
    return;
  }
  auto now = std::chrono::high_resolution_clock::now();
  double frameTime = std::chrono::duration<double, std::milli>(now - lastFrameStart_).count();
  frameTimes_.push_back(frameTime);
  if (static_cast<int>(frameTimes_.size()) > sampleSize_) {
    frameTimes_.pop_front();
  }
  frameStarted_ = false;
}

ProfileSection RenderProfiler::timeSection(const std::string& name) {
  return ProfileSection(this, name);
}

void RenderProfiler::recordTime(const std::string& name, double elapsedMs) {
  if (!enabled) {
    return;
  }
  auto& stats = timings_[name];
  stats.maxSamples = static_cast<size_t>(sampleSize_);
  stats.record(elapsedMs);
}

std::string RenderProfiler::getStats() const {
  if (timings_.empty()) {
    return "No profiling data";
  }

  std::ostringstream oss;
  oss << std::string(70, '=') << "\n";
  oss << "RENDER PROFILING STATS (times in milliseconds)\n";
  oss << std::string(70, '=') << "\n";

  if (!frameTimes_.empty()) {
    double avgFrame = std::accumulate(frameTimes_.begin(), frameTimes_.end(), 0.0) /
        static_cast<double>(frameTimes_.size());
    double minFrame = *std::min_element(frameTimes_.begin(), frameTimes_.end());
    double maxFrame = *std::max_element(frameTimes_.begin(), frameTimes_.end());
    double fps = avgFrame > 0.0 ? 1000.0 / avgFrame : 0.0;

    std::array<char, 256> buf{};
    std::snprintf(
        buf.data(),
        buf.size(),
        "TOTAL FRAME:  avg=%6.2fms  min=%6.2fms  max=%6.2fms  fps=%5.1f",
        avgFrame,
        minFrame,
        maxFrame,
        fps);
    oss << buf.data() << "\n";
    oss << std::string(70, '-') << "\n";
  }

  struct SectionInfo {
    std::string name;
    double avg, minVal, maxVal;
  };
  std::vector<SectionInfo> sections;
  for (const auto& [name, stats] : timings_) {
    if (!stats.samples.empty()) {
      sections.push_back({name, stats.average(), stats.minVal, stats.maxVal});
    }
  }
  std::sort(
      sections.begin(), sections.end(), [](const auto& a, const auto& b) { return a.avg > b.avg; });

  for (const auto& s : sections) {
    std::array<char, 256> buf{};
    std::snprintf(
        buf.data(),
        buf.size(),
        "%-30s  avg=%6.2fms  min=%6.2fms  max=%6.2fms",
        s.name.c_str(),
        s.avg,
        s.minVal,
        s.maxVal);
    oss << buf.data() << "\n";
  }

  oss << std::string(70, '=');
  return oss.str();
}

void RenderProfiler::printStats() const {
  if (enabled) {
    std::printf("%s\n", getStats().c_str());
  }
}

void RenderProfiler::reset() {
  timings_.clear();
  frameTimes_.clear();
}

} // namespace noodles
