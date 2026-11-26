/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dlfcn.h>
#include <link.h>
#include <stdlib.h>
#include <android/dlext.h>
#include <unordered_map>
#include <string>

#if defined(_WIN32)
#include "elf/guest_images.h"
#endif

// Elixir (Windows): COFF weak externals never pull in the defining object
// from a static library and resolve to absolute zero, so on Windows these are
// ordinary declarations/definitions.
#if defined(_WIN32)
#define ELIXIR_LIBDL_WEAK_DEFAULT __attribute__((visibility("default")))
#define ELIXIR_LIBDL_WEAK
#else
#define ELIXIR_LIBDL_WEAK_DEFAULT ELIXIR_LIBDL_WEAK_DEFAULT
#define ELIXIR_LIBDL_WEAK ELIXIR_LIBDL_WEAK
#endif

// These functions are exported by the loader
// TODO(dimitry): replace these with reference to libc.so

extern "C" {

ELIXIR_LIBDL_WEAK_DEFAULT
void __loader_android_get_LD_LIBRARY_PATH(char* buffer, size_t buffer_size);

ELIXIR_LIBDL_WEAK_DEFAULT
void* __loader_dlopen(const char* filename, int flags, const void* caller_addr);

ELIXIR_LIBDL_WEAK_DEFAULT
char* __loader_dlerror();

ELIXIR_LIBDL_WEAK_DEFAULT
void* __loader_dlsym(void* handle, const char* symbol, const void* caller_addr);

ELIXIR_LIBDL_WEAK_DEFAULT
void* __loader_dlvsym(void* handle,
                      const char* symbol,
                      const char* version,
                      const void* caller_addr);

ELIXIR_LIBDL_WEAK_DEFAULT
int __loader_dladdr(const void* addr, Dl_info* info);

ELIXIR_LIBDL_WEAK_DEFAULT
int __loader_dlclose(void* handle);

#if defined(__arm__)
ELIXIR_LIBDL_WEAK_DEFAULT
_Unwind_Ptr __loader_dl_unwind_find_exidx(_Unwind_Ptr pc, int* pcount);
#endif

ELIXIR_LIBDL_WEAK_DEFAULT
int __loader_dl_iterate_phdr(int (ELIXIR_GUEST_CC *cb)(struct dl_phdr_info* info, size_t size, void* data),
                             void* data);

ELIXIR_LIBDL_WEAK_DEFAULT
void __loader_android_get_LD_LIBRARY_PATH(char* buffer, size_t buffer_size);

ELIXIR_LIBDL_WEAK_DEFAULT
void __loader_android_update_LD_LIBRARY_PATH(const char* ld_library_path);

ELIXIR_LIBDL_WEAK_DEFAULT
void* __loader_android_dlopen_ext(const char* filename,
                                  int flag,
                                  const android_dlextinfo* extinfo,
                                  const void* caller_addr);

ELIXIR_LIBDL_WEAK_DEFAULT
int __loader_android_get_application_target_sdk_version();

// Proxy calls to bionic loader
ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
void android_get_LD_LIBRARY_PATH(char* buffer, size_t buffer_size) {
  __loader_android_get_LD_LIBRARY_PATH(buffer, buffer_size);
}

} // extern "C"

namespace linker::libdl {

#if defined(_WIN32)
// Elixir (Windows): the functions below are what guest code calls. Guest
// code lives in the Android tree, so it is told a library's name there
// ("/data/app/<package>/lib/libroblox.so", the name /proc/self/maps shows
// for the same file) and may open a library by such a name; the loader
// itself works with host paths. See src/elf/guest_images.h.
namespace {

struct ElixirGuestIteration {
  int (ELIXIR_GUEST_CC* callback)(struct dl_phdr_info* info, size_t size, void* data);
  void* data;
};

int ELIXIR_GUEST_CC ElixirReportWithGuestName(struct dl_phdr_info* info, size_t size, void* data) {
  auto* iteration = static_cast<ElixirGuestIteration*>(data);
  struct dl_phdr_info named = *info;
  named.dlpi_name = elixir::elf::GuestNameOf(info->dlpi_name);
  return iteration->callback(&named, size, iteration->data);
}

}  // namespace
#endif

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
void* dlopen(const char* filename, int flag) {
  const void* caller_addr = __builtin_return_address(0);
#if defined(_WIN32)
  std::string host_path;
  if (elixir::elf::HostPathOf(filename, &host_path)) return __loader_dlopen(host_path.c_str(), flag, caller_addr);
#endif
  return __loader_dlopen(filename, flag, caller_addr);
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
char* dlerror() {
  return __loader_dlerror();
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
void* dlsym(void* handle, const char* symbol) {
  const void* caller_addr = __builtin_return_address(0);
  return __loader_dlsym(handle, symbol, caller_addr);
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
void* dlvsym(void* handle, const char* symbol, const char* version) {
  const void* caller_addr = __builtin_return_address(0);
  return __loader_dlvsym(handle, symbol, version, caller_addr);
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
int dladdr(const void* addr, Dl_info* info) {
  const int found = __loader_dladdr(addr, info);
#if defined(_WIN32)
  if (found != 0 && info != nullptr) info->dli_fname = elixir::elf::GuestNameOf(info->dli_fname);
#endif
  return found;
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
int dlclose(void* handle) {
  return __loader_dlclose(handle);
}

#if defined(__arm__)
ELIXIR_LIBDL_WEAK
_Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int* pcount) {
  return __loader_dl_unwind_find_exidx(pc, pcount);
}
#endif

/*
 * This needs to be defined as weak because it is also defined in libc.a.
 * Without this, static executables will have a multiple definition error.
 */
ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
int dl_iterate_phdr(int (ELIXIR_GUEST_CC *cb)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
#if defined(_WIN32)
  ElixirGuestIteration iteration{cb, data};
  return __loader_dl_iterate_phdr(&ElixirReportWithGuestName, &iteration);
#else
  return __loader_dl_iterate_phdr(cb, data);
#endif
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
void* android_dlopen_ext(const char* filename, int flag, const android_dlextinfo* extinfo) {
  const void* caller_addr = __builtin_return_address(0);
#if defined(_WIN32)
  std::string host_path;
  if (elixir::elf::HostPathOf(filename, &host_path)) {
    return __loader_android_dlopen_ext(host_path.c_str(), flag, extinfo, caller_addr);
  }
#endif
  return __loader_android_dlopen_ext(filename, flag, extinfo, caller_addr);
}

ELIXIR_LIBDL_WEAK ELIXIR_GUEST_CC
int android_get_application_target_sdk_version() {
  return __loader_android_get_application_target_sdk_version();
}

#if defined(_WIN32)
// Elixir: cross-DSO control-flow integrity (-fsanitize-cfi-cross-dso) needs
// the linker's CFI shadow, which Elixir does not build. As on Android when
// no loaded library uses CFI, every target counts as unchecked: the slow
// path returns and __cfi_shadow_size() reports no shadow. This removes a
// hardening check; it does not change program behaviour.
ELIXIR_GUEST_CC size_t elixir_libdl___cfi_shadow_size() { return 0; }
ELIXIR_GUEST_CC void elixir_libdl___cfi_slowpath(uint64_t /*call_site_type_id*/, void* /*target*/) {}
ELIXIR_GUEST_CC void elixir_libdl___cfi_slowpath_diag(uint64_t /*call_site_type_id*/, void* /*target*/,
                                                      void* /*diag_data*/) {}
#endif

#if defined(__arm__)
// An arm32 unwinding table has an R_ARM_NONE relocation to
// __aeabi_unwind_cpp_pr0. This shared library will never invoke the unwinder,
// so it doesn't actually need the routine. Define a dummy version here,
// because the real version calls libc functions (e.g. memcpy, abort), which
// would create a dependency cycle with libc.so.
__attribute__((visibility("hidden")))
void __aeabi_unwind_cpp_pr0() {
  __builtin_trap();
}
#endif

std::unordered_map<std::string, void *> get_dl_symbols() {
    return {
        {"dlopen", (void *) dlopen},
        {"dlerror", (void *) dlerror},
        {"dlsym", (void *) dlsym},
        {"dlvsym", (void *) dlvsym},
        {"dladdr", (void *) dladdr},
        {"dlclose", (void *) dlclose},
#if defined(__arm__)
        {"dl_unwind_find_exidx", (void *) dl_unwind_find_exidx},
#endif
        {"dl_iterate_phdr", (void *) dl_iterate_phdr},
        {"android_dlopen_ext", (void *) android_dlopen_ext},
        {"android_get_application_target_sdk_version", (void *) android_get_application_target_sdk_version},
#if defined(_WIN32)
        {"__cfi_shadow_size", (void *) elixir_libdl___cfi_shadow_size},
        {"__cfi_slowpath", (void *) elixir_libdl___cfi_slowpath},
        {"__cfi_slowpath_diag", (void *) elixir_libdl___cfi_slowpath_diag},
#endif
#if defined(__arm__)
        {"__aeabi_unwind_cpp_pr0", (void *) __aeabi_unwind_cpp_pr0},
#endif
    };
}

}