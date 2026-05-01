// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_GLYPH_H
#define NOODLES_CORE_GLYPH_H

#include "core/Math.h"
#include "core/api.h"
#include "core/pragmas.h"

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

struct NOODLES_API Glyph {
  int unicode = 0;
  double advance = 0.0;
  Range2d planeBounds;
  Range2d atlasBounds;
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_CORE_GLYPH_H
