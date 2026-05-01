// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "core/Animator.h"

#include <algorithm>
#include <cmath>

namespace noodles {

AnimatableProperty* Animator::addProperty() {
  properties_.emplace_back();
  auto* ptr = &properties_.back();
  propertyPtrs_.push_back(ptr);
  return ptr;
}

void Animator::removeProperty(AnimatableProperty* prop) {
  for (size_t i = 0; i < properties_.size(); ++i) {
    if (&properties_[i] == prop) {
      properties_.erase(properties_.begin() + static_cast<long>(i));
      // Rebuild pointer vector
      propertyPtrs_.clear();
      for (auto& p : properties_) {
        propertyPtrs_.push_back(&p);
      }
      return;
    }
  }
}

void Animator::update(double dt) {
  bool stillAnimating = false;

  for (auto& prop : properties_) {
    double s = 1.0 - std::exp(-dt * prop.speed);
    prop.current = prop.current + s * (prop.target - prop.current);

    if (std::abs(prop.current - prop.target) > TOLERANCE) {
      stillAnimating = true;
    } else {
      prop.current = prop.target;
    }
  }

  animating_ = stillAnimating;
}

} // namespace noodles
