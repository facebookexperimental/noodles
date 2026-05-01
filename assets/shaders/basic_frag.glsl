// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#version 330 core

out vec4 FragColor;

uniform vec4 uColor;

void main() {
    FragColor = uColor;
}
