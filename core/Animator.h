// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_ANIMATOR_H
#define NOODLES_CORE_ANIMATOR_H

#include "core/api.h"
#include "core/pragmas.h"

#include <deque>
#include <vector>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API AnimatableProperty {
  double current = 0.0;
  double target = 0.0;
  double speed = 1.0;
};

class NOODLES_API Animator {
 public:
  static constexpr double TOLERANCE = 1e-6;

  AnimatableProperty* addProperty();
  void removeProperty(AnimatableProperty* prop);
  void update(double dt);
  bool isAnimating() const {
    return animating_;
  }

  const std::vector<AnimatableProperty*>& properties() const {
    return propertyPtrs_;
  }

 private:
  std::deque<AnimatableProperty> properties_;
  std::vector<AnimatableProperty*> propertyPtrs_;
  bool animating_ = false;
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_CORE_ANIMATOR_H
