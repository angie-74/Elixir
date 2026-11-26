// Modified by Elixir.
#ifndef MOCKTAIL_COMPAT_GUEST_ABI_H_
#define MOCKTAIL_COMPAT_GUEST_ABI_H_

#if defined(_WIN32)
#ifndef EM_X86_64
#define EM_X86_64 62
#endif
#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif
#else
#include <elf.h>
#endif
#include <string_view>

namespace mocktail::compat {

#if defined(__aarch64__)
inline constexpr std::string_view kGuestAbi = "arm64-v8a";
inline constexpr std::string_view kGuestApkSplitName = "config.arm64_v8a";
inline constexpr std::string_view kGuestSplitApkFile =
    "split_config.arm64_v8a.apk";
inline constexpr std::string_view kGuestSplitApkHashKey = "arm64_v8a_split_apk";
inline constexpr std::string_view kGuestCpuName = "aarch64";
inline constexpr int kGuestElfMachine = EM_AARCH64;
#elif defined(__x86_64__)
inline constexpr std::string_view kGuestAbi = "x86_64";
inline constexpr std::string_view kGuestApkSplitName = "config.x86_64";
inline constexpr std::string_view kGuestSplitApkFile = "split_config.x86_64.apk";
inline constexpr std::string_view kGuestSplitApkHashKey = "x86_64_split_apk";
inline constexpr std::string_view kGuestCpuName = "x86_64";
inline constexpr int kGuestElfMachine = EM_X86_64;
#else
#error "mocktail only supports aarch64 and x86_64 hosts"
#endif

inline constexpr std::string_view kGuestLibApkEntryPrefix = "lib/";

}

#endif
