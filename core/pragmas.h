// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_PRAGMAS_H
#define NOODLES_CORE_PRAGMAS_H

#ifdef _MSC_VER
  #define NOODLES_PRAGMA_PUSH __pragma(warning(push))
  #define NOODLES_PRAGMA_POP __pragma(warning(pop))
  #define NOODLES_PRAGMA_EXPORT_INTERFACE __pragma(warning(disable : 4251))
#else
  #define NOODLES_PRAGMA_PUSH
  #define NOODLES_PRAGMA_POP
  #define NOODLES_PRAGMA_EXPORT_INTERFACE
#endif

#endif // NOODLES_CORE_PRAGMAS_H
