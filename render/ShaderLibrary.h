// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_RENDER_SHADER_LIBRARY_H
#define NOODLES_RENDER_SHADER_LIBRARY_H

#include "core/api.h"
#include "core/pragmas.h"

#include <GL/glew.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace noodles {

NOODLES_PRAGMA_PUSH
NOODLES_PRAGMA_EXPORT_INTERFACE

class NOODLES_API GLSLProgram {
 public:
  GLSLProgram(
      const std::string& vsSource,
      const std::string& fsSource,
      const std::vector<std::string>& uniformNames);
  ~GLSLProgram();

  GLSLProgram(const GLSLProgram&) = delete;
  GLSLProgram& operator=(const GLSLProgram&) = delete;
  GLSLProgram(GLSLProgram&&) = delete;
  GLSLProgram& operator=(GLSLProgram&&) = delete;

  void use();
  void release();
  GLint getUniformLocation(const std::string& name) const;
  GLuint program() const {
    return program_;
  }

 private:
  GLuint program_ = 0;
  std::unordered_map<std::string, GLint> uniformLocations_;
};

class NOODLES_API ShaderLibrary {
 public:
  ShaderLibrary() = default;
  ~ShaderLibrary();

  ShaderLibrary(const ShaderLibrary&) = delete;
  ShaderLibrary& operator=(const ShaderLibrary&) = delete;
  ShaderLibrary(ShaderLibrary&&) = delete;
  ShaderLibrary& operator=(ShaderLibrary&&) = delete;

  void initialize(const std::string& assetsPath);
  GLSLProgram* get(const std::string& name);
  void cleanup();

 private:
  std::unordered_map<std::string, std::unique_ptr<GLSLProgram>> shaders_;

  GLSLProgram* compileShader(
      const std::string& vsPath,
      const std::string& fsPath,
      const std::vector<std::string>& uniformNames);
};

NOODLES_PRAGMA_POP

} // namespace noodles

#endif // NOODLES_RENDER_SHADER_LIBRARY_H
