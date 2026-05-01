// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_CORE_API_H
#define NOODLES_CORE_API_H

#if defined(PXR_STATIC) || defined(NOODLES_STATIC)
  #define NOODLES_API
  #define NOODLES_API_TEMPLATE_CLASS(...)
  #define NOODLES_API_TEMPLATE_STRUCT(...)
  #define NOODLES_LOCAL
#else
  #if defined(_WIN32)
    #if defined(NOODLES_EXPORTS)
      #define NOODLES_API __declspec(dllexport)
      #define NOODLES_API_TEMPLATE_CLASS(...) template class __declspec(dllexport) __VA_ARGS__
      #define NOODLES_API_TEMPLATE_STRUCT(...) template struct __declspec(dllexport) __VA_ARGS__
    #else
      #define NOODLES_API __declspec(dllimport)
      #define NOODLES_API_TEMPLATE_CLASS(...) template class __declspec(dllimport) __VA_ARGS__
      #define NOODLES_API_TEMPLATE_STRUCT(...) template struct __declspec(dllimport) __VA_ARGS__
    #endif
    #define NOODLES_LOCAL
  #else
    #if defined(NOODLES_EXPORTS)
      #define NOODLES_API __attribute__((visibility("default")))
      #define NOODLES_API_TEMPLATE_CLASS(...) \
        template class __attribute__((visibility("default"))) __VA_ARGS__
      #define NOODLES_API_TEMPLATE_STRUCT(...) \
        template struct __attribute__((visibility("default"))) __VA_ARGS__
    #else
      #define NOODLES_API
      #define NOODLES_API_TEMPLATE_CLASS(...)
      #define NOODLES_API_TEMPLATE_STRUCT(...)
    #endif
    #define NOODLES_LOCAL __attribute__((visibility("hidden")))
  #endif
#endif

#endif // NOODLES_CORE_API_H
