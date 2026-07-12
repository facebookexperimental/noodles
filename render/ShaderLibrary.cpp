// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "render/ShaderLibrary.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace noodles {

namespace {

/// Normalize path separators to the platform's native format.
/// On Windows, \\?\ extended-length paths require all backslashes.
std::string normalizePath(const std::string& path) {
#ifdef _WIN32
  std::string result = path;
  std::replace(result.begin(), result.end(), '/', '\\');
  return result;
#else
  return path;
#endif
}

struct ShaderDef {
  std::string name;
  std::string vertFile;
  std::string fragFile;
  std::vector<std::string> uniforms;
};

const std::vector<ShaderDef>& getShaderDefs() {
  static const std::vector<ShaderDef> defs = {
      {"node",
       "node_vert.glsl",
       "node_frag.glsl",
       {"uProjection", "uCornerRadius", "uInnerStrokeColor", "uNodeTransforms"}},
      {"link_poly",
       "link_poly_vert.glsl",
       "link_poly_frag.glsl",
       {"uProjection",
        "uThickness",
        "uZoom",
        "uViewportCenter",
        "uMaxViewportDim",
        "uDimmingStart",
        "uDimmingEnd",
        "uCutoffAlpha",
        "uDimming",
        "uAlphaBase",
        "uLinkColor",
        "uSelectedLinkColor",
        "uHoveredLinkColor"}},
      {"text",
       "msdf_vert.glsl",
       "msdf_frag.glsl",
       {"uProjection", "uAtlas", "uTextColor", "uPxRange", "uNodeTransforms"}},
      {"icon", "icon_vert.glsl", "icon_frag.glsl", {"uProjection", "uTexture", "uNodeTransforms"}},
  };
  return defs;
}

std::string readFile(const std::string& path) {
  std::string nativePath = normalizePath(path);
  std::ifstream file(nativePath);
  if (!file.is_open()) {
    return {};
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

} // anonymous namespace

// GLSLProgram

GLSLProgram::GLSLProgram(
    const std::string& vsSource,
    const std::string& fsSource,
    const std::vector<std::string>& uniformNames) {
  program_ = glCreateProgram();
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  const char* vsSrc = vsSource.c_str();
  glShaderSource(vertexShader, 1, &vsSrc, nullptr);
  glCompileShader(vertexShader);

  GLint vsCompileStatus = 0;
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vsCompileStatus);
  if (vsCompileStatus == GL_FALSE) {
    GLint logLen = 0;
    glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(logLen, '\0');
    glGetShaderInfoLog(vertexShader, logLen, nullptr, &log[0]);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(program_);
    program_ = 0;
    return;
  }

  const char* fsSrc = fsSource.c_str();
  glShaderSource(fragmentShader, 1, &fsSrc, nullptr);
  glCompileShader(fragmentShader);

  GLint fsCompileStatus = 0;
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fsCompileStatus);
  if (fsCompileStatus == GL_FALSE) {
    GLint logLen = 0;
    glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(logLen, '\0');
    glGetShaderInfoLog(fragmentShader, logLen, nullptr, &log[0]);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(program_);
    program_ = 0;
    return;
  }

  glAttachShader(program_, vertexShader);
  glAttachShader(program_, fragmentShader);
  glLinkProgram(program_);

  GLint linkStatus = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &linkStatus);
  if (linkStatus == GL_FALSE) {
    GLint logLen = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(logLen, '\0');
    glGetProgramInfoLog(program_, logLen, nullptr, &log[0]);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(program_);
    program_ = 0;
    return;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  for (const auto& name : uniformNames) {
    uniformLocations_[name] = glGetUniformLocation(program_, name.c_str());
  }
}

GLSLProgram::~GLSLProgram() {
  if (program_) {
    glDeleteProgram(program_);
  }
}

void GLSLProgram::use() {
  glUseProgram(program_);
}

void GLSLProgram::release() {
  glUseProgram(0);
}

GLint GLSLProgram::getUniformLocation(const std::string& name) const {
  auto it = uniformLocations_.find(name);
  if (it != uniformLocations_.end()) {
    return it->second;
  }
  return -1;
}

// ShaderLibrary

// Best-effort cleanup: if cleanup() was already called while the GL context
// was current (the expected path via GraphRenderer::cleanup()), this is a
// no-op on an empty map.  If not, the GLSLProgram destructors will call
// glDeleteProgram() which requires a valid GL context -- undefined behavior
// if the context is already torn down.
ShaderLibrary::~ShaderLibrary() {
  cleanup();
}

void ShaderLibrary::initialize(const std::string& assetsPath) {
  std::string shadersPath = normalizePath(assetsPath + "/shaders/");

  for (const auto& def : getShaderDefs()) {
    std::string vsPath = shadersPath + def.vertFile;
    std::string fsPath = shadersPath + def.fragFile;

    auto* shader = compileShader(vsPath, fsPath, def.uniforms);
    if (shader) {
      shaders_[def.name] = std::unique_ptr<GLSLProgram>(shader);
    }
  }
}

GLSLProgram* ShaderLibrary::get(const std::string& name) {
  auto it = shaders_.find(name);
  if (it != shaders_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void ShaderLibrary::cleanup() {
  shaders_.clear();
}

GLSLProgram* ShaderLibrary::compileShader(
    const std::string& vsPath,
    const std::string& fsPath,
    const std::vector<std::string>& uniformNames) {
  std::string vsSource = readFile(vsPath);
  std::string fsSource = readFile(fsPath);
  if (vsSource.empty() || fsSource.empty()) {
    return nullptr;
  }

  auto* program = new GLSLProgram(vsSource, fsSource, uniformNames);
  if (program->program() == 0) {
    delete program;
    return nullptr;
  }
  return program;
}

} // namespace noodles
