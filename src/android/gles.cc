// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libgles_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "android/android_internal.h"
#include "bionic/runtime.h"

namespace {

using GLbitfield = std::uint32_t;
using GLboolean = std::uint8_t;
using GLchar = char;
using GLenum = std::uint32_t;
using GLfloat = float;
using GLint = std::int32_t;
using GLsizei = std::int32_t;
using GLuint = std::uint32_t;
using GLsizeiptr = std::int64_t;
using GLintptr = std::int64_t;

constexpr GLenum kGlNoError = 0;
constexpr GLenum kGlInvalidOperation = 0x0502;
constexpr GLenum kGlFramebufferUnsupported = 0x8cdd;

thread_local GLenum t_last_error = kGlNoError;

void Unavailable() { t_last_error = kGlInvalidOperation; }

void ZeroObjects(GLsizei n, GLuint* objects) {
  if (objects == nullptr || n <= 0) return;
  Unavailable();
  std::memset(objects, 0, static_cast<std::size_t>(n) * sizeof(*objects));
}

void CopyString(const char* source, GLsizei buffer_size, GLsizei* length, GLchar* out) {
  const GLsizei source_length = source == nullptr ? 0 : static_cast<GLsizei>(std::strlen(source));
  if (length != nullptr) *length = source_length;
  if (out == nullptr || buffer_size <= 0) return;
  const GLsizei copy = source_length < buffer_size - 1 ? source_length : buffer_size - 1;
  if (copy > 0) std::memcpy(out, source, static_cast<std::size_t>(copy));
  out[copy] = '\0';
}

}

#define GL_FN(ret, name) ELIXIR_ANDROID_FN(ret, name)

GL_FN(const std::uint8_t*, glGetString)(GLenum name) noexcept {
  Unavailable();
  return nullptr;
}

GL_FN(GLenum, glGetError)() noexcept {
  const GLenum error = t_last_error;
  t_last_error = kGlNoError;
  return error;
}

GL_FN(void, glGetIntegerv)(GLenum pname, GLint* data) noexcept {
  if (data == nullptr) return;
  Unavailable();
  *data = 0;
}

GL_FN(void, glGenTextures)(GLsizei n, GLuint* textures) noexcept { ZeroObjects(n, textures); }
GL_FN(void, glGenBuffers)(GLsizei n, GLuint* buffers) noexcept { ZeroObjects(n, buffers); }
GL_FN(void, glGenFramebuffers)(GLsizei n, GLuint* framebuffers) noexcept { ZeroObjects(n, framebuffers); }
GL_FN(void, glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers) noexcept { ZeroObjects(n, renderbuffers); }

GL_FN(GLuint, glCreateShader)(GLenum type) noexcept {
  Unavailable();
  return 0;
}

GL_FN(GLuint, glCreateProgram)() noexcept {
  Unavailable();
  return 0;
}

GL_FN(void, glGetShaderiv)(GLuint shader, GLenum pname, GLint* params) noexcept {
  if (params == nullptr) return;
  Unavailable();
  *params = 0;
}

GL_FN(void, glGetProgramiv)(GLuint program, GLenum pname, GLint* params) noexcept {
  if (params == nullptr) return;
  Unavailable();
  *params = 0;
}

GL_FN(void, glGetShaderInfoLog)(GLuint shader, GLsizei buffer_size, GLsizei* length, GLchar* log) noexcept {
  Unavailable();
  CopyString("real GLES backend unavailable", buffer_size, length, log);
}

GL_FN(void, glGetProgramInfoLog)(GLuint program, GLsizei buffer_size, GLsizei* length, GLchar* log) noexcept {
  Unavailable();
  CopyString("real GLES backend unavailable", buffer_size, length, log);
}

GL_FN(void, glGetActiveUniform)(GLuint program, GLuint index, GLsizei buffer_size, GLsizei* length, GLint* size,
                                GLenum* type, GLchar* name) noexcept {
  Unavailable();
  if (size != nullptr) *size = 0;
  if (type != nullptr) *type = 0;
  CopyString("", buffer_size, length, name);
}

GL_FN(GLint, glGetUniformLocation)(GLuint program, const GLchar* name) noexcept {
  Unavailable();
  return -1;
}

GL_FN(GLenum, glCheckFramebufferStatus)(GLenum target) noexcept {
  Unavailable();
  return kGlFramebufferUnsupported;
}

GL_FN(void, glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                          void* pixels) noexcept {
  Unavailable();
}

GL_FN(void, glViewport)(GLint, GLint, GLsizei, GLsizei) noexcept { Unavailable(); }
GL_FN(void, glActiveTexture)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glAttachShader)(GLuint, GLuint) noexcept { Unavailable(); }
GL_FN(void, glBindAttribLocation)(GLuint, GLuint, const GLchar*) noexcept { Unavailable(); }
GL_FN(void, glBindBuffer)(GLenum, GLuint) noexcept { Unavailable(); }
GL_FN(void, glBindFramebuffer)(GLenum, GLuint) noexcept { Unavailable(); }
GL_FN(void, glBindRenderbuffer)(GLenum, GLuint) noexcept { Unavailable(); }
GL_FN(void, glBindTexture)(GLenum, GLuint) noexcept { Unavailable(); }
GL_FN(void, glBlendFunc)(GLenum, GLenum) noexcept { Unavailable(); }
GL_FN(void, glBlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum) noexcept { Unavailable(); }
GL_FN(void, glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) noexcept { Unavailable(); }
GL_FN(void, glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) noexcept { Unavailable(); }
GL_FN(void, glClear)(GLbitfield) noexcept { Unavailable(); }
GL_FN(void, glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) noexcept { Unavailable(); }
GL_FN(void, glClearDepthf)(GLfloat) noexcept { Unavailable(); }
GL_FN(void, glClearStencil)(GLint) noexcept { Unavailable(); }
GL_FN(void, glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) noexcept { Unavailable(); }
GL_FN(void, glCompileShader)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*) noexcept {
  Unavailable();
}
GL_FN(void, glCompressedTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei,
                                       const void*) noexcept {
  Unavailable();
}
GL_FN(void, glCopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) noexcept {
  Unavailable();
}
GL_FN(void, glCullFace)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glDeleteBuffers)(GLsizei, const GLuint*) noexcept { Unavailable(); }
GL_FN(void, glDeleteFramebuffers)(GLsizei, const GLuint*) noexcept { Unavailable(); }
GL_FN(void, glDeleteProgram)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glDeleteRenderbuffers)(GLsizei, const GLuint*) noexcept { Unavailable(); }
GL_FN(void, glDeleteShader)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glDeleteTextures)(GLsizei, const GLuint*) noexcept { Unavailable(); }
GL_FN(void, glDepthFunc)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glDepthMask)(GLboolean) noexcept { Unavailable(); }
GL_FN(void, glDisable)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glDisableVertexAttribArray)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glDrawArrays)(GLenum, GLint, GLsizei) noexcept { Unavailable(); }
GL_FN(void, glDrawElements)(GLenum, GLsizei, GLenum, const void*) noexcept { Unavailable(); }
GL_FN(void, glEnable)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glEnableVertexAttribArray)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint) noexcept { Unavailable(); }
GL_FN(void, glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) noexcept { Unavailable(); }
GL_FN(void, glGenerateMipmap)(GLenum) noexcept { Unavailable(); }
GL_FN(void, glLinkProgram)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glPixelStorei)(GLenum, GLint) noexcept { Unavailable(); }
GL_FN(void, glPolygonOffset)(GLfloat, GLfloat) noexcept { Unavailable(); }
GL_FN(void, glReleaseShaderCompiler)() noexcept { Unavailable(); }
GL_FN(void, glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei) noexcept { Unavailable(); }
GL_FN(void, glScissor)(GLint, GLint, GLsizei, GLsizei) noexcept { Unavailable(); }
GL_FN(void, glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) noexcept { Unavailable(); }
GL_FN(void, glStencilFunc)(GLenum, GLint, GLuint) noexcept { Unavailable(); }
GL_FN(void, glStencilMask)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glStencilOp)(GLenum, GLenum, GLenum) noexcept { Unavailable(); }
GL_FN(void, glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) noexcept {
  Unavailable();
}
GL_FN(void, glTexParameterf)(GLenum, GLenum, GLfloat) noexcept { Unavailable(); }
GL_FN(void, glTexParameterfv)(GLenum, GLenum, const GLfloat*) noexcept { Unavailable(); }
GL_FN(void, glTexParameteri)(GLenum, GLenum, GLint) noexcept { Unavailable(); }
GL_FN(void, glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) noexcept {
  Unavailable();
}
GL_FN(void, glUniform1i)(GLint, GLint) noexcept { Unavailable(); }
GL_FN(void, glUseProgram)(GLuint) noexcept { Unavailable(); }
GL_FN(void, glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) noexcept {
  Unavailable();
}

#undef GL_FN

namespace elixir::android {
namespace {

struct GlEntry {
  const char* name;
  void* address;
};

#define ELIXIR_GL(name) GlEntry{#name, reinterpret_cast<void*>(&elixir_android_##name)}
const GlEntry kGlEntries[] = {
    ELIXIR_GL(glGetString),          ELIXIR_GL(glGetError),
    ELIXIR_GL(glGetIntegerv),        ELIXIR_GL(glGenTextures),
    ELIXIR_GL(glGenBuffers),         ELIXIR_GL(glGenFramebuffers),
    ELIXIR_GL(glGenRenderbuffers),   ELIXIR_GL(glCreateShader),
    ELIXIR_GL(glCreateProgram),      ELIXIR_GL(glGetShaderiv),
    ELIXIR_GL(glGetProgramiv),       ELIXIR_GL(glGetShaderInfoLog),
    ELIXIR_GL(glGetProgramInfoLog),  ELIXIR_GL(glGetActiveUniform),
    ELIXIR_GL(glGetUniformLocation), ELIXIR_GL(glCheckFramebufferStatus),
    ELIXIR_GL(glViewport),           ELIXIR_GL(glActiveTexture),
    ELIXIR_GL(glAttachShader),       ELIXIR_GL(glBindAttribLocation),
    ELIXIR_GL(glBindBuffer),         ELIXIR_GL(glBindFramebuffer),
    ELIXIR_GL(glBindRenderbuffer),   ELIXIR_GL(glBindTexture),
    ELIXIR_GL(glBlendFunc),          ELIXIR_GL(glBlendFuncSeparate),
    ELIXIR_GL(glBufferData),         ELIXIR_GL(glBufferSubData),
    ELIXIR_GL(glClear),              ELIXIR_GL(glClearColor),
    ELIXIR_GL(glClearDepthf),        ELIXIR_GL(glClearStencil),
    ELIXIR_GL(glColorMask),          ELIXIR_GL(glCompileShader),
    ELIXIR_GL(glCompressedTexImage2D), ELIXIR_GL(glCompressedTexSubImage2D),
    ELIXIR_GL(glCopyTexSubImage2D),  ELIXIR_GL(glCullFace),
    ELIXIR_GL(glDeleteBuffers),      ELIXIR_GL(glDeleteFramebuffers),
    ELIXIR_GL(glDeleteProgram),      ELIXIR_GL(glDeleteRenderbuffers),
    ELIXIR_GL(glDeleteShader),       ELIXIR_GL(glDeleteTextures),
    ELIXIR_GL(glDepthFunc),          ELIXIR_GL(glDepthMask),
    ELIXIR_GL(glDisable),            ELIXIR_GL(glDisableVertexAttribArray),
    ELIXIR_GL(glDrawArrays),         ELIXIR_GL(glDrawElements),
    ELIXIR_GL(glEnable),             ELIXIR_GL(glEnableVertexAttribArray),
    ELIXIR_GL(glFramebufferRenderbuffer), ELIXIR_GL(glFramebufferTexture2D),
    ELIXIR_GL(glGenerateMipmap),     ELIXIR_GL(glLinkProgram),
    ELIXIR_GL(glPixelStorei),        ELIXIR_GL(glPolygonOffset),
    ELIXIR_GL(glReadPixels),         ELIXIR_GL(glReleaseShaderCompiler),
    ELIXIR_GL(glRenderbufferStorage), ELIXIR_GL(glScissor),
    ELIXIR_GL(glShaderSource),       ELIXIR_GL(glStencilFunc),
    ELIXIR_GL(glStencilMask),        ELIXIR_GL(glStencilOp),
    ELIXIR_GL(glTexImage2D),         ELIXIR_GL(glTexParameterf),
    ELIXIR_GL(glTexParameterfv),     ELIXIR_GL(glTexParameteri),
    ELIXIR_GL(glTexSubImage2D),      ELIXIR_GL(glUniform1i),
    ELIXIR_GL(glUseProgram),         ELIXIR_GL(glVertexAttribPointer),
};
#undef ELIXIR_GL

}

void* FindGlesEntryPoint(const char* name) noexcept {
  if (void* egl = FindEglEntryPoint(name); egl != nullptr) return egl;
  for (const GlEntry& entry : kGlEntries) {
    if (std::strcmp(entry.name, name) == 0) return entry.address;
  }
  return nullptr;
}

void RegisterGlesExports(bionic::ExportTable& table) {
  for (const GlEntry& entry : kGlEntries) {
    table.Add(kLibGlesV2, entry.name, entry.address);
    table.Add(kLibGlesV3, entry.name, entry.address);
  }
}

}
