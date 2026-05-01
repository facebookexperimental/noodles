// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

in float vAlpha;
out vec4 outColor;

void main() {
    outColor = vec4(0.35, 0.36, 0.37, vAlpha);
}
