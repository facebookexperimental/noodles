// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_LINK_CURVE_PARAMS_H
#define NOODLES_RENDER_LINK_CURVE_PARAMS_H

namespace noodles {

// Shaping constants for the link "noodle" curve.
//
// These MUST stay in sync with the GPU vertex shader
// assets/shaders/link_poly_vert.glsl, which performs the identical shaping at
// render time. LinkGeometry::sampleLinkCurve reproduces that shader on the CPU
// so the pickable curve matches what is drawn. There is no compile-time link
// between this header and the GLSL source: if you change a value here, change it
// in link_poly_vert.glsl too (and vice versa) or hit-testing will silently
// drift from the rendered curve. The parity is guarded by the golden tests in
// tests/noodles/NoodlesRenderTest.cpp.

// Forward<->reverse transition span (world units) and the offset that starts the
// transition slightly before the x zero-crossing.
inline constexpr double kLinkTransitionSpan = 500.0; // glsl: tFR
inline constexpr double kLinkTransitionOffset = 50.0; // glsl: tOffset

// Minimum vertical separation below which endpoints are treated as "close".
inline constexpr double kLinkYMinDist = 200.0; // glsl: yMinDist

// How sharply the backward S-curve bows toward the midpoint.
inline constexpr double kLinkMidSharpness = 0.75; // glsl: midSharpness

// Backward-curve horizontal handle: clamp((start.x - end.x) * factor, min, span).
inline constexpr double kLinkBackwardHandleFactor = 0.1;
inline constexpr double kLinkBackwardHandleMin = 100.0;

// Floor on the backward-curve vertical bow, in the direction of travel.
inline constexpr double kLinkBackwardYOffMin = 100.0;

// mx blend = clamp((start.x - end.x + ...) * factor, 0, span) / span.
inline constexpr double kLinkTransitionMxFactor = 0.5;

// Vertical-end-tangent (prim-target) handle: clamp(|scale| * factor, min, max).
inline constexpr double kLinkEndHandleFactor = 0.35;
inline constexpr double kLinkEndHandleMin = 20.0;
inline constexpr double kLinkEndHandleMax = 120.0;

// Below this |scale.x| the horizontal handle direction defaults to +1.
inline constexpr double kLinkHorizontalDirEpsilon = 0.0001;

// Prim-target arrow inset (screen-scaled): max(factor / zoom, min) world units.
inline constexpr double kLinkPrimTargetArrowFactor = 10.0;
inline constexpr double kLinkPrimTargetArrowMin = 2.0;

} // namespace noodles

#endif // NOODLES_RENDER_LINK_CURVE_PARAMS_H
