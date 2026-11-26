// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_JNIVM_JNI_ABI_H_
#define MOCKTAIL_JNIVM_JNI_ABI_H_

#include <jni.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_WIN32)
#include <optional>

#include "abi/sysv_va_list.h"
#endif

#if defined(_WIN32)
#define ELIXIR_JNI_LAMBDA_CC __attribute__((sysv_abi)) noexcept
#else
#define ELIXIR_JNI_LAMBDA_CC
#endif

#if defined(_WIN32) && !defined(ELIXIR_JNI_SYSV)
#error "jnivm on Windows must use third_party/jni/include/jni.h (JNICALL = sysv_abi)"
#endif

namespace jnivm {
namespace internal {

#if defined(_WIN32)
template <typename T>
struct SysvFunctionPointerTraits {
  static constexpr bool kIsSysv = false;
  static constexpr bool kVariadic = false;
};
template <typename R, typename... A>
struct SysvFunctionPointerTraits<R(__attribute__((sysv_abi)) *)(A...)> {
  static constexpr bool kIsSysv = true;
  static constexpr bool kVariadic = false;
};
template <typename R, typename... A>
struct SysvFunctionPointerTraits<R(__attribute__((sysv_abi)) *)(A...) noexcept> {
  static constexpr bool kIsSysv = true;
  static constexpr bool kVariadic = false;
};
template <typename R, typename... A>
struct SysvFunctionPointerTraits<R(__attribute__((sysv_abi)) *)(A..., ...)> {
  static constexpr bool kIsSysv = true;
  static constexpr bool kVariadic = true;
  static constexpr std::size_t kFixed = sizeof...(A);
  using VaImpl = R(__attribute__((sysv_abi)) *)(A..., elixir::abi::SysvVaList*) noexcept;
};

template <typename Slot>
Slot VariadicThunkAs(void(__attribute__((sysv_abi)) * thunk)()) noexcept {
  static_assert(SysvFunctionPointerTraits<Slot>::kIsSysv &&
                    SysvFunctionPointerTraits<Slot>::kVariadic,
                "variadic thunks only fill System V variadic JNI slots");
  return reinterpret_cast<Slot>(thunk);
}

static_assert(SysvFunctionPointerTraits<void(JNICALL*)()>::kIsSysv,
              "JNICALL must be sysv_abi on Windows");
static_assert(!SysvFunctionPointerTraits<void (*)()>::kIsSysv,
              "the trait must reject Microsoft x64 pointers");
static_assert(SysvFunctionPointerTraits<decltype(JNINativeInterface_::GetVersion)>::kIsSysv &&
                  SysvFunctionPointerTraits<decltype(JNINativeInterface_::NewObject)>::kVariadic &&
                  SysvFunctionPointerTraits<decltype(JNINativeInterface_::CallFloatMethodV)>::kIsSysv &&
                  SysvFunctionPointerTraits<decltype(JNINativeInterface_::CallStaticDoubleMethodA)>::kIsSysv &&
                  SysvFunctionPointerTraits<decltype(JNINativeInterface_::GetModule)>::kIsSysv &&
                  SysvFunctionPointerTraits<decltype(JNIInvokeInterface_::DestroyJavaVM)>::kIsSysv &&
                  SysvFunctionPointerTraits<decltype(JNIInvokeInterface_::AttachCurrentThreadAsDaemon)>::kIsSysv,
              "JNI table members must be System V function pointers");
#endif

class JniSignatureCursor {
 public:
  explicit JniSignatureCursor(const char* signature) noexcept
      : cursor_(signature != nullptr && signature[0] == '(' ? signature + 1
                                                            : nullptr) {}

  char Next() noexcept {
    if (cursor_ == nullptr) {
      return '\0';
    }
    const char kind = *cursor_;
    switch (kind) {
      case 'Z': case 'B': case 'C': case 'S':
      case 'I': case 'J': case 'F': case 'D':
        ++cursor_;
        return kind;
      case 'L':
        return SkipClassName() ? 'L' : Stop();
      case '[': {
        while (*cursor_ == '[') {
          ++cursor_;
        }
        if (*cursor_ == 'L') {
          return SkipClassName() ? 'L' : Stop();
        }
        if (*cursor_ == '\0' || std::strchr("ZBCSIJFD", *cursor_) == nullptr) {
          return Stop();
        }
        ++cursor_;
        return 'L';
      }
      default:
        return Stop();
    }
  }

 private:
  bool SkipClassName() noexcept {
    const char* end = std::strchr(cursor_, ';');
    if (end == nullptr) {
      return false;
    }
    cursor_ = end + 1;
    return true;
  }
  char Stop() noexcept {
    cursor_ = nullptr;
    return '\0';
  }

  const char* cursor_;
};

class JniVaArgs {
 public:
  JniVaArgs(const char* signature, va_list args) noexcept
      : signature_(signature) {
#if defined(_WIN32)
    if (args != nullptr) {
      cursor_.emplace(reinterpret_cast<elixir::abi::SysvVaList*>(args));
    }
#else
    va_copy(list_, args);
#endif
  }

  ~JniVaArgs() {
#if !defined(_WIN32)
    va_end(list_);
#endif
  }

  JniVaArgs(const JniVaArgs&) = delete;
  JniVaArgs& operator=(const JniVaArgs&) = delete;

  template <typename T>
  T Next() noexcept {
    char kind = signature_.Next();
    if (kind == '\0') {
      kind = FallbackKind<T>();
    }
    return Convert<T>(Read(kind), kind);
  }

  std::size_t DecodeRemaining(jvalue* out, std::size_t capacity) noexcept {
    std::size_t count = 0;
    while (count < capacity) {
      const char kind = signature_.Next();
      if (kind == '\0') {
        break;
      }
      out[count++] = Read(kind);
    }
    return count;
  }

 private:
  template <typename T>
  static constexpr char FallbackKind() noexcept {
    if constexpr (std::is_pointer_v<T>) {
      return 'L';
    } else if constexpr (std::is_floating_point_v<T>) {
      return 'D';
    } else {
      static_assert(std::is_integral_v<T>, "JNI arguments are scalars");
      return sizeof(T) > sizeof(jint) ? 'J' : 'I';
    }
  }

  static std::int64_t IntegerValue(const jvalue& value, char kind) noexcept {
    switch (kind) {
      case 'Z': return value.z;
      case 'B': return value.b;
      case 'C': return value.c;
      case 'S': return value.s;
      case 'I': return value.i;
      case 'J': return value.j;
      case 'F': return Truncate(value.f);
      case 'D': return Truncate(value.d);
      default:
        return static_cast<std::int64_t>(
            reinterpret_cast<std::intptr_t>(value.l));
    }
  }

  static double FloatingValue(const jvalue& value, char kind) noexcept {
    if (kind == 'F') return value.f;
    if (kind == 'D') return value.d;
    return static_cast<double>(IntegerValue(value, kind));
  }

  static std::int64_t Truncate(double value) noexcept {
    return value > -9.2e18 && value < 9.2e18 ? static_cast<std::int64_t>(value)
                                             : 0;
  }

  template <typename T>
  static T Convert(const jvalue& value, char kind) noexcept {
    if constexpr (std::is_pointer_v<T>) {
      if (kind == 'L') return reinterpret_cast<T>(value.l);
      if (kind == 'F' || kind == 'D') return nullptr;
      return reinterpret_cast<T>(
          static_cast<std::intptr_t>(IntegerValue(value, kind)));
    } else if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(FloatingValue(value, kind));
    } else {
      return static_cast<T>(IntegerValue(value, kind));
    }
  }

  jvalue Read(char kind) noexcept {
    jvalue value;
    value.j = 0;
#if defined(_WIN32)
    if (!cursor_.has_value()) {
      return value;
    }
    switch (kind) {
      case 'F': value.f = static_cast<jfloat>(cursor_->NextDouble()); break;
      case 'D': value.d = cursor_->NextDouble(); break;
      default: {
        const std::uint64_t raw = cursor_->NextGp();
        switch (kind) {
          case 'Z': value.z = static_cast<jboolean>(raw); break;
          case 'B': value.b = static_cast<jbyte>(raw); break;
          case 'C': value.c = static_cast<jchar>(raw); break;
          case 'S': value.s = static_cast<jshort>(raw); break;
          case 'I': value.i = static_cast<jint>(static_cast<std::uint32_t>(raw)); break;
          case 'J': value.j = static_cast<jlong>(raw); break;
          default:
            value.l = reinterpret_cast<jobject>(static_cast<std::uintptr_t>(raw));
            break;
        }
        break;
      }
    }
#else
    switch (kind) {
      case 'Z': value.z = static_cast<jboolean>(va_arg(list_, int)); break;
      case 'B': value.b = static_cast<jbyte>(va_arg(list_, int)); break;
      case 'C': value.c = static_cast<jchar>(va_arg(list_, int)); break;
      case 'S': value.s = static_cast<jshort>(va_arg(list_, int)); break;
      case 'I': value.i = va_arg(list_, jint); break;
      case 'J': value.j = va_arg(list_, jlong); break;
      case 'F': value.f = static_cast<jfloat>(va_arg(list_, double)); break;
      case 'D': value.d = va_arg(list_, double); break;
      default: value.l = va_arg(list_, jobject); break;
    }
#endif
    return value;
  }

  JniSignatureCursor signature_;
#if defined(_WIN32)
  std::optional<elixir::abi::SysvVaCursor> cursor_;
#else
  va_list list_;
#endif
};

inline std::size_t JValuesFromVaList(const char* signature, va_list args,
                                     jvalue* out,
                                     std::size_t capacity) noexcept {
  JniVaArgs reader(signature, args);
  return reader.DecodeRemaining(out, capacity);
}

}
}

#endif
