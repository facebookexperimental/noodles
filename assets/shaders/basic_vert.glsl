// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * vec4(aPosition, 1.0);
}
