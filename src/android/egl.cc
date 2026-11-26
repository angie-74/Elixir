// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libegl_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <cstdint>
#include <cstring>

#include "android/android_internal.h"
#include "bionic/runtime.h"

namespace {

using EGLBoolean = std::uint32_t;
using EGLint = std::int32_t;
using EGLenum = std::uint32_t;
using EGLDisplay = void*;
using EGLConfig = void*;
using EGLContext = void*;
using EGLSurface = void*;

constexpr EGLBoolean kEglTrue = 1;
constexpr EGLBoolean kEglFalse = 0;
constexpr EGLint kEglSuccess = 0x3000;
constexpr EGLint kEglNotInitialized = 0x3001;
constexpr EGLint kEglBadAttribute = 0x3004;
constexpr EGLint kEglBadDisplay = 0x3008;
constexpr EGLenum kEglOpenGlesApi = 0x30a0;
constexpr EGLint kEglReadSurface = 0x305A;

thread_local EGLint t_last_error = kEglSuccess;
thread_local EGLContext t_current_context = nullptr;
thread_local EGLSurface t_current_draw = nullptr;
thread_local EGLSurface t_current_read = nullptr;

constexpr EGLDisplay kActiveDisplay = nullptr;

void SetError(EGLint error) { t_last_error = error; }

}

ELIXIR_ANDROID_FN(EGLDisplay, eglGetDisplay)(void* native_display) noexcept {
  SetError(kEglNotInitialized);
  return kActiveDisplay;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglInitialize)(EGLDisplay display, EGLint* major, EGLint* minor) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglBindAPI)(EGLenum api) noexcept {
  if (api != kEglOpenGlesApi) {
    SetError(kEglBadAttribute);
    return kEglFalse;
  }
  SetError(kEglSuccess);
  return kEglTrue;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglChooseConfig)(EGLDisplay display, const EGLint* attributes, EGLConfig* configs,
                                               EGLint config_size, EGLint* num_config) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglGetConfigAttrib)(EGLDisplay display, EGLConfig config, EGLint attribute,
                                                  EGLint* value) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLSurface, eglCreateWindowSurface)(EGLDisplay display, EGLConfig config, void* native_window,
                                                      const EGLint* attributes) noexcept {
  SetError(kEglBadDisplay);
  return nullptr;
}

ELIXIR_ANDROID_FN(EGLSurface, eglCreatePbufferSurface)(EGLDisplay display, EGLConfig config,
                                                       const EGLint* attributes) noexcept {
  SetError(kEglBadDisplay);
  return nullptr;
}

ELIXIR_ANDROID_FN(EGLContext, eglCreateContext)(EGLDisplay display, EGLConfig config, EGLContext share,
                                                const EGLint* attributes) noexcept {
  SetError(kEglBadDisplay);
  return nullptr;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglMakeCurrent)(EGLDisplay display, EGLSurface draw, EGLSurface read,
                                              EGLContext context) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLContext, eglGetCurrentContext)() noexcept { return t_current_context; }

ELIXIR_ANDROID_FN(EGLSurface, eglGetCurrentSurface)(EGLint read_draw) noexcept {
  return read_draw == kEglReadSurface ? t_current_read : t_current_draw;
}

ELIXIR_ANDROID_FN(EGLDisplay, eglGetCurrentDisplay)() noexcept {
  return t_current_context != nullptr ? kActiveDisplay : nullptr;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglSwapBuffers)(EGLDisplay display, EGLSurface surface) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglSwapInterval)(EGLDisplay display, EGLint interval) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglQuerySurface)(EGLDisplay display, EGLSurface surface, EGLint attribute,
                                               EGLint* value) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(const char*, eglQueryString)(EGLDisplay display, EGLint name) noexcept {
  SetError(kEglBadDisplay);
  return nullptr;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglDestroyContext)(EGLDisplay display, EGLContext context) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglDestroySurface)(EGLDisplay display, EGLSurface surface) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLBoolean, eglTerminate)(EGLDisplay display) noexcept {
  SetError(kEglBadDisplay);
  return kEglFalse;
}

ELIXIR_ANDROID_FN(EGLint, eglGetError)() noexcept {
  const EGLint error = t_last_error;
  t_last_error = kEglSuccess;
  return error;
}

ELIXIR_ANDROID_FN(void*, eglGetProcAddress)(const char* name) noexcept {
  if (name == nullptr) return nullptr;
  if (std::strcmp(name, "eglGetProcAddress") == 0) return reinterpret_cast<void*>(&elixir_android_eglGetProcAddress);
  return elixir::android::FindGlesEntryPoint(name);
}

namespace elixir::android {
namespace {

struct EglEntry {
  const char* name;
  void* address;
};

#define ELIXIR_EGL(name) EglEntry{#name, reinterpret_cast<void*>(&elixir_android_##name)}
const EglEntry kEglEntries[] = {
    ELIXIR_EGL(eglBindAPI),          ELIXIR_EGL(eglChooseConfig),       ELIXIR_EGL(eglCreateContext),
    ELIXIR_EGL(eglCreatePbufferSurface), ELIXIR_EGL(eglCreateWindowSurface), ELIXIR_EGL(eglDestroyContext),
    ELIXIR_EGL(eglDestroySurface),   ELIXIR_EGL(eglGetConfigAttrib),    ELIXIR_EGL(eglGetCurrentContext),
    ELIXIR_EGL(eglGetCurrentDisplay), ELIXIR_EGL(eglGetCurrentSurface), ELIXIR_EGL(eglGetDisplay),
    ELIXIR_EGL(eglGetError),         ELIXIR_EGL(eglInitialize),         ELIXIR_EGL(eglMakeCurrent),
    ELIXIR_EGL(eglQueryString),      ELIXIR_EGL(eglQuerySurface),       ELIXIR_EGL(eglSwapBuffers),
    ELIXIR_EGL(eglSwapInterval),     ELIXIR_EGL(eglTerminate),
};
#undef ELIXIR_EGL

}

void* FindEglEntryPoint(const char* name) noexcept {
  for (const EglEntry& entry : kEglEntries) {
    if (std::strcmp(entry.name, name) == 0) return entry.address;
  }
  return nullptr;
}

void RegisterEglExports(bionic::ExportTable& table) {
  for (const EglEntry& entry : kEglEntries) table.Add(kLibEgl, entry.name, entry.address);
  table.Add(kLibEgl, "eglGetProcAddress", reinterpret_cast<void*>(&elixir_android_eglGetProcAddress));
}

}
