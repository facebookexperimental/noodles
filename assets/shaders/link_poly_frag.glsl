// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

in float vAlpha;
in float vSelected;
in float vHovered;
in float vDir;
out vec4 outColor;

uniform vec3 uLinkColor;
uniform vec3 uSelectedLinkColor;
uniform vec3 uHoveredLinkColor;

void main() {
    float d = abs(vDir) - 0.5;
    float pixWidth = length(vec2(dFdx(vDir), dFdy(vDir)));
    float alpha = smoothstep(d-pixWidth, d+pixWidth, 0.0);

    // 3-state blend: normal -> hovered -> selected (selected takes priority)
    vec3 baseColor = mix(uLinkColor, uHoveredLinkColor, vHovered);
    float baseAlpha = mix(vAlpha, 1.0, vHovered);
    outColor = mix(vec4(baseColor, baseAlpha * alpha), vec4(uSelectedLinkColor, alpha), vSelected);
}
