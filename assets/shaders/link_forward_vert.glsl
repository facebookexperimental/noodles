// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

layout(location = 0) in vec2 refPosition;
layout(location = 1) in vec2 startPoint;
layout(location = 2) in vec2 endPoint;
layout(location = 3) in float alpha;

uniform mat4 uProjection;

out float vAlpha;

void main() {
    vec2 scale = endPoint - startPoint;
    vec2 finalPosition = startPoint + refPosition * scale;

    gl_Position = uProjection * vec4(finalPosition, 0.0, 1.0);

    //const float PI = 3.1415926535897932384626433832795;
    //vAlpha = alpha * (cos(refPosition.x * PI * 2.0) * 0.25 + 0.75);

    vAlpha = alpha;
}
