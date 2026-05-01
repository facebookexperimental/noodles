// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aLocalCoord;
layout(location = 2) in vec2 aRectSize;
layout(location = 3) in vec4 aColor;
layout(location = 4) in float aInnerStroke;
layout(location = 5) in vec4 aSelectedColor;
layout(location = 6) in float aSelected;

uniform mat4 uProjection;
uniform float uCornerRadius;

out vec4 fragColor;
out vec4 fragSelectedColor;
out float fragSelected;
out vec2 fragLocalCoord;
out vec2 fragRectSize;
out float fragInnerStroke;

void main() {
    // Expand quad by a fixed amount (corner radius) to provide room for
    // anti-aliased rounded corners and strokes.  Previously this used
    // 1% of aRectSize which caused sub-quads (e.g. title bar) that
    // reference the full node size to expand by different amounts
    // depending on the total node height.
    float expand = max(uCornerRadius, 4.0);
    vec2 expandFrac = expand / max(aRectSize, vec2(1.0));

    vec3 pos = aPosition;
    if (aLocalCoord.x < 0.5 * aRectSize.x) {
        pos.x -= expand;
    } else {
        pos.x += expand;
    }
    if (aLocalCoord.y < 0.5 * aRectSize.y) {
        pos.y -= expand;
    } else {
        pos.y += expand;
    }
    gl_Position = uProjection * vec4(pos, 1.0);

    fragColor = aColor;
    fragSelectedColor = aSelectedColor;
    fragSelected = aSelected;
    fragRectSize = aRectSize;
    fragInnerStroke = aInnerStroke;

    // Remap UV coordinates to account for the fixed expansion.
    // Each side expands by expandFrac (in UV space), so total scale is 1 + 2*expandFrac.
    vec2 scale = vec2(1.0) + 2.0 * expandFrac;
    fragLocalCoord = aLocalCoord * scale - expandFrac * aRectSize;
}
