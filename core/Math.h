// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_MATH_H
#define NOODLES_CORE_MATH_H

#include <algorithm>
#include <array>
#include <cmath>

namespace noodles {

struct Vec2d {
  std::array<double, 2> data{0.0, 0.0};

  Vec2d() = default;
  Vec2d(double x, double y) : data{x, y} {}

  double operator[](int i) const {
    return data[i];
  }
  double& operator[](int i) {
    return data[i];
  }

  Vec2d operator+(const Vec2d& o) const {
    return {data[0] + o[0], data[1] + o[1]};
  }
  Vec2d operator-(const Vec2d& o) const {
    return {data[0] - o[0], data[1] - o[1]};
  }
  bool operator==(const Vec2d& o) const {
    return data[0] == o[0] && data[1] == o[1];
  }
  bool operator!=(const Vec2d& o) const {
    return !(*this == o);
  }
};

struct Vec2f {
  std::array<float, 2> data{0.0f, 0.0f};

  Vec2f() = default;
  Vec2f(float x, float y) : data{x, y} {}

  float operator[](int i) const {
    return data[i];
  }
  float& operator[](int i) {
    return data[i];
  }
};

struct Vec4f {
  std::array<float, 4> data{0.0f, 0.0f, 0.0f, 0.0f};

  Vec4f() = default;
  Vec4f(float x, float y, float z, float w) : data{x, y, z, w} {}

  float operator[](int i) const {
    return data[i];
  }
  float& operator[](int i) {
    return data[i];
  }
};

class Range2d {
 public:
  Range2d() : min_(0, 0), max_(0, 0), empty_(true) {}
  Range2d(const Vec2d& minVal, const Vec2d& maxVal)
      : min_(minVal), max_(maxVal), empty_(minVal[0] > maxVal[0] || minVal[1] > maxVal[1]) {}

  const Vec2d& GetMin() const {
    return min_;
  }
  const Vec2d& GetMax() const {
    return max_;
  }

  Vec2d GetSize() const {
    if (empty_) {
      return {0, 0};
    }
    return max_ - min_;
  }

  bool IsEmpty() const {
    return empty_;
  }

  bool Contains(const Vec2d& point) const {
    if (empty_) {
      return false;
    }
    return point[0] >= min_[0] && point[0] <= max_[0] && point[1] >= min_[1] && point[1] <= max_[1];
  }

  bool operator==(const Range2d& o) const {
    if (empty_ && o.empty_) {
      return true;
    }
    if (empty_ != o.empty_) {
      return false;
    }
    return min_ == o.min_ && max_ == o.max_;
  }
  bool operator!=(const Range2d& o) const {
    return !(*this == o);
  }

  static Range2d GetIntersection(const Range2d& a, const Range2d& b) {
    if (a.empty_ || b.empty_) {
      return {};
    }
    double minX = std::max(a.min_[0], b.min_[0]);
    double minY = std::max(a.min_[1], b.min_[1]);
    double maxX = std::min(a.max_[0], b.max_[0]);
    double maxY = std::min(a.max_[1], b.max_[1]);
    if (minX > maxX || minY > maxY) {
      return {};
    }
    return {Vec2d(minX, minY), Vec2d(maxX, maxY)};
  }

  static Range2d GetUnion(const Range2d& a, const Range2d& b) {
    if (a.empty_) {
      return b;
    }
    if (b.empty_) {
      return a;
    }
    return {
        Vec2d(std::min(a.min_[0], b.min_[0]), std::min(a.min_[1], b.min_[1])),
        Vec2d(std::max(a.max_[0], b.max_[0]), std::max(a.max_[1], b.max_[1]))};
  }

 private:
  Vec2d min_;
  Vec2d max_;
  bool empty_;
};

} // namespace noodles

#endif // NOODLES_CORE_MATH_H
