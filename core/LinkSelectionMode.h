// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_LINK_SELECTION_MODE_H
#define NOODLES_CORE_LINK_SELECTION_MODE_H

#include "core/api.h"

namespace noodles {

enum class NOODLES_API LinkSelectionMode : int {
  NODES_ONLY = 0,
  WITH_ALL_LINKS = 1,
  WITH_INPUTS = 2,
  WITH_OUTPUTS = 3,
};

} // namespace noodles

#endif // NOODLES_CORE_LINK_SELECTION_MODE_H
