// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in float aNodeIndex;

uniform mat4 uProjection;
uniform sampler1D uNodeTransforms;
uniform float uTransformTextureSize;

out vec2 vTexCoord;

void main() {
    vec3 transformedPos = aPosition;

    // Only apply transform for non-negative node indices
    // Negative indices (like -1.0) are used for UI overlays that shouldn't move
    if (aNodeIndex >= 0.0) {
        // Read node transform from texture (1D texture indexed by node index)
        // Each texel stores (dx, dy) as RG components
        float texCoord = (aNodeIndex + 0.5) / uTransformTextureSize;
        vec2 offset = texture(uNodeTransforms, texCoord).rg;
        transformedPos = vec3(aPosition.xy + offset, aPosition.z);
    }

    gl_Position = uProjection * vec4(transformedPos, 1.0);
    vTexCoord = aTexCoord;
}
