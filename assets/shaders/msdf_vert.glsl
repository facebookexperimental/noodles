// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in float aNodeIndex;

uniform mat4 uProjection;
uniform samplerBuffer uNodeTransforms;

out vec2 vTexCoord;

void main() {
    vec3 transformedPos = aPosition;

    // Negative indices skip the fetch (UI overlays that shouldn't move).
    if (aNodeIndex >= 0.0) {
        vec2 offset = texelFetch(uNodeTransforms, int(aNodeIndex)).rg;
        transformedPos = vec3(aPosition.xy + offset, aPosition.z);
    }

    gl_Position = uProjection * vec4(transformedPos, 1.0);
    vTexCoord = aTexCoord;
}
