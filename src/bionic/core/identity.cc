// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Follows bionic's libc/bionic/grp_pwd.cpp and private/grp_pwd.h:
//
//   Copyright (C) 2008 The Android Open Source Project (grp_pwd.cpp)
//   Copyright (C) 2017 The Android Open Source Project (grp_pwd.h)
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
//   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
//   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
//
// The AID names and values are those of AOSP
// system/core/libcutils/include/private/android_filesystem_config.h
// (Copyright (C) 2007 The Android Open Source Project, Apache-2.0).
#include "bionic/core/process_ext.h"

#include <windows.h>

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>

#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/core/properties.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(int, setgroups)(abi::g_size count, const abi::g_gid* groups) noexcept;

namespace core {
namespace {

struct GuestPasswd {
  char* pw_name;
  char* pw_passwd;
  std::uint32_t pw_uid;
  std::uint32_t pw_gid;
  char* pw_gecos;
  char* pw_dir;
  char* pw_shell;
};
static_assert(sizeof(GuestPasswd) == 48, "struct passwd");
static_assert(offsetof(GuestPasswd, pw_uid) == 16 && offsetof(GuestPasswd, pw_gecos) == 24 &&
                  offsetof(GuestPasswd, pw_dir) == 32 && offsetof(GuestPasswd, pw_shell) == 40,
              "struct passwd fields");

struct GuestGroup {
  char* gr_name;
  char* gr_passwd;
  std::uint32_t gr_gid;
  char** gr_mem;
};
static_assert(sizeof(GuestGroup) == 32, "struct group");
static_assert(offsetof(GuestGroup, gr_gid) == 16 && offsetof(GuestGroup, gr_mem) == 24, "struct group fields");

constexpr std::size_t kGrpPwdBufferSize = 32;

struct GroupState {
  GuestGroup group_;
  char* group_members_[2];
  char group_name_buffer_[kGrpPwdBufferSize];
  abi::g_ssize getgrent_idx;
};

struct PasswdState {
  GuestPasswd passwd_;
  char name_buffer_[kGrpPwdBufferSize];
  char dir_buffer_[kGrpPwdBufferSize];
  char sh_buffer_[kGrpPwdBufferSize];
  abi::g_ssize getpwent_idx;
};

constexpr std::uint32_t kAidSystem = 1000, kAidRadio = 1001, kAidBluetooth = 1002, kAidLog = 1007,
                        kAidNfc = 1027, kAidShell = 2000, kAidSecureElement = 1068, kAidNetworkStack = 1073;
constexpr std::uint32_t kAidOemReservedStart = 2900, kAidOemReservedEnd = 2999;
constexpr std::uint32_t kAidOemReserved2Start = 5000, kAidOemReserved2End = 5999;
constexpr std::uint32_t kAidSystemReservedStart = 6000, kAidSystemExtReservedEnd = 7999;
constexpr std::uint32_t kAidEverybody = 9997;
constexpr std::uint32_t kAidAppStart = 10000, kAidAppEnd = 19999;
constexpr std::uint32_t kAidCacheGidStart = 20000, kAidCacheGidEnd = 29999;
constexpr std::uint32_t kAidExtGidStart = 30000, kAidExtGidEnd = 39999;
constexpr std::uint32_t kAidExtCacheGidStart = 40000, kAidExtCacheGidEnd = 49999;
constexpr std::uint32_t kAidSharedGidStart = 50000, kAidSharedGidEnd = 59999;
constexpr std::uint32_t kAidOverflowUid = 65534;
constexpr std::uint32_t kAidIsolatedStart = 90000, kAidIsolatedEnd = 99999;
constexpr std::uint32_t kAidUserOffset = 100000;

struct AndroidIdInfo {
  const char* name;
  std::uint32_t aid;
};

constexpr AndroidIdInfo kAndroidIds[] = {
    {"root", 0},
    {"daemon", 1},
    {"bin", 2},
    {"sys", 3},
    {"system", 1000},
    {"radio", 1001},
    {"bluetooth", 1002},
    {"graphics", 1003},
    {"input", 1004},
    {"audio", 1005},
    {"camera", 1006},
    {"log", 1007},
    {"compass", 1008},
    {"mount", 1009},
    {"wifi", 1010},
    {"adb", 1011},
    {"install", 1012},
    {"media", 1013},
    {"dhcp", 1014},
    {"sdcard_rw", 1015},
    {"vpn", 1016},
    {"keystore", 1017},
    {"usb", 1018},
    {"drm", 1019},
    {"mdnsr", 1020},
    {"gps", 1021},
    {"media_rw", 1023},
    {"mtp", 1024},
    {"drmrpc", 1026},
    {"nfc", 1027},
    {"sdcard_r", 1028},
    {"clat", 1029},
    {"loop_radio", 1030},
    {"mediadrm", 1031},
    {"package_info", 1032},
    {"sdcard_pics", 1033},
    {"sdcard_av", 1034},
    {"sdcard_all", 1035},
    {"logd", 1036},
    {"shared_relro", 1037},
    {"dbus", 1038},
    {"tlsdate", 1039},
    {"mediaex", 1040},
    {"audioserver", 1041},
    {"metrics_coll", 1042},
    {"metricsd", 1043},
    {"webserv", 1044},
    {"debuggerd", 1045},
    {"mediacodec", 1046},
    {"cameraserver", 1047},
    {"firewall", 1048},
    {"trunks", 1049},
    {"nvram", 1050},
    {"dns", 1051},
    {"dns_tether", 1052},
    {"webview_zygote", 1053},
    {"vehicle_network", 1054},
    {"media_audio", 1055},
    {"media_video", 1056},
    {"media_image", 1057},
    {"tombstoned", 1058},
    {"media_obb", 1059},
    {"ese", 1060},
    {"ota_update", 1061},
    {"automotive_evs", 1062},
    {"lowpan", 1063},
    {"hsm", 1064},
    {"reserved_disk", 1065},
    {"statsd", 1066},
    {"incidentd", 1067},
    {"secure_element", 1068},
    {"lmkd", 1069},
    {"llkd", 1070},
    {"iorapd", 1071},
    {"gpu_service", 1072},
    {"network_stack", 1073},
    {"gsid", 1074},
    {"fsverity_cert", 1075},
    {"credstore", 1076},
    {"external_storage", 1077},
    {"ext_data_rw", 1078},
    {"ext_obb_rw", 1079},
    {"context_hub", 1080},
    {"virtualizationservice", 1081},
    {"artd", 1082},
    {"uwb", 1083},
    {"thread_network", 1084},
    {"diced", 1085},
    {"dmesgd", 1086},
    {"jc_weaver", 1087},
    {"jc_strongbox", 1088},
    {"jc_identitycred", 1089},
    {"sdk_sandbox", 1090},
    {"security_log_writer", 1091},
    {"prng_seeder", 1092},
    {"uprobestats", 1093},
    {"shell", 2000},
    {"cache", 2001},
    {"diag", 2002},
    {"net_bt_admin", 3001},
    {"net_bt", 3002},
    {"inet", 3003},
    {"net_raw", 3004},
    {"net_admin", 3005},
    {"net_bw_stats", 3006},
    {"net_bw_acct", 3007},
    {"readproc", 3009},
    {"wakelock", 3010},
    {"uhid", 3011},
    {"readtracefs", 3012},
    {"virtualmachine", 3013},
    {"everybody", 9997},
    {"misc", 9998},
    {"nobody", 9999},
    {"overflowuid", 65534},
};
constexpr std::size_t kAndroidIdCount = sizeof(kAndroidIds) / sizeof(kAndroidIds[0]);

struct IdRange {
  std::uint32_t start;
  std::uint32_t end;
};
constexpr IdRange kUserRanges[] = {
    {kAidAppStart, kAidAppEnd},
    {kAidIsolatedStart, kAidIsolatedEnd},
};
constexpr IdRange kGroupRanges[] = {
    {kAidAppStart, kAidAppEnd},
    {kAidCacheGidStart, kAidCacheGidEnd},
    {kAidExtGidStart, kAidExtGidEnd},
    {kAidExtCacheGidStart, kAidExtCacheGidEnd},
    {kAidSharedGidStart, kAidSharedGidEnd},
    {kAidIsolatedStart, kAidIsolatedEnd},
};

constexpr std::uint32_t kSecondaryUserPlatformIds[] = {
    kAidSystem, kAidRadio, kAidLog, kAidNfc, kAidBluetooth, kAidShell, kAidSecureElement, kAidNetworkStack,
};

thread_local PasswdState t_passwd_state;
thread_local GroupState t_group_state;

void InitGroupState(GroupState* state) noexcept {
  std::memset(state, 0, sizeof(GroupState) - sizeof(state->getgrent_idx));
  state->group_.gr_name = state->group_name_buffer_;
  state->group_.gr_mem = state->group_members_;
  state->group_.gr_mem[0] = state->group_.gr_name;
}

GroupState* GroupTlsBuffer() noexcept {
  InitGroupState(&t_group_state);
  return &t_group_state;
}

void InitPasswdState(PasswdState* state) noexcept {
  std::memset(state, 0, sizeof(PasswdState) - sizeof(state->getpwent_idx));
  state->passwd_.pw_name = state->name_buffer_;
  state->passwd_.pw_dir = state->dir_buffer_;
  state->passwd_.pw_shell = state->sh_buffer_;
}

PasswdState* PasswdTlsBuffer() noexcept {
  InitPasswdState(&t_passwd_state);
  return &t_passwd_state;
}

GuestPasswd* IdInfoToPasswd(PasswdState* state, const AndroidIdInfo* info) noexcept {
  std::snprintf(state->name_buffer_, sizeof(state->name_buffer_), "%s", info->name);
  std::snprintf(state->dir_buffer_, sizeof(state->dir_buffer_), "/");
  std::snprintf(state->sh_buffer_, sizeof(state->sh_buffer_), "/bin/sh");
  GuestPasswd* pw = &state->passwd_;
  pw->pw_uid = info->aid;
  pw->pw_gid = info->aid;
  return pw;
}

GuestGroup* IdInfoToGroup(GroupState* state, const AndroidIdInfo* info) noexcept {
  std::snprintf(state->group_name_buffer_, sizeof(state->group_name_buffer_), "%s", info->name);
  GuestGroup* gr = &state->group_;
  gr->gr_gid = info->aid;
  return gr;
}

const AndroidIdInfo* FindAndroidIdInfo(std::uint32_t id) noexcept {
  for (const AndroidIdInfo& info : kAndroidIds) {
    if (info.aid == id) return &info;
  }
  return nullptr;
}

const AndroidIdInfo* FindAndroidIdInfo(const char* name) noexcept {
  for (const AndroidIdInfo& info : kAndroidIds) {
    if (std::strcmp(info.name, name) == 0) return &info;
  }
  return nullptr;
}

bool PlatformIdSecondaryUserAllowed(std::uint32_t id) noexcept {
  for (const std::uint32_t allowed : kSecondaryUserPlatformIds) {
    if (allowed == id) return true;
  }
  return false;
}

bool IsValidAppId(std::uint32_t id, bool is_group) noexcept {
  const std::uint32_t appid = id % kAidUserOffset;
  if (appid == kAidOverflowUid) return false;
  const IdRange* ranges = is_group ? kGroupRanges : kUserRanges;
  const std::size_t count = is_group ? std::size(kGroupRanges) : std::size(kUserRanges);
  if (appid < ranges[0].start && PlatformIdSecondaryUserAllowed(appid)) return true;
  if (appid >= kAidSharedGidStart && appid <= kAidSharedGidEnd && appid != id) return false;
  for (std::size_t i = 0; i < count; ++i) {
    if (appid >= ranges[i].start && appid <= ranges[i].end) return true;
  }
  return false;
}

abi::g_ssize NextAppId(abi::g_ssize current, bool is_group) noexcept {
  const IdRange* ranges = is_group ? kGroupRanges : kUserRanges;
  const std::size_t count = is_group ? std::size(kGroupRanges) : std::size(kUserRanges);
  if (current < static_cast<abi::g_ssize>(ranges[0].start)) return ranges[0].start;
  const abi::g_ssize incremented = current + 1;
  for (std::size_t i = 1; i < count; ++i) {
    if (incremented > static_cast<abi::g_ssize>(ranges[i - 1].end) &&
        incremented < static_cast<abi::g_ssize>(ranges[i].start)) {
      return ranges[i].start;
    }
  }
  if (incremented > static_cast<abi::g_ssize>(ranges[count - 1].end)) return -1;
  return incremented;
}

std::uint32_t AppIdFromName(const char* name, bool is_group) noexcept {
  const char* end;
  std::uint64_t userid;
  bool is_shared_gid = false;
  if (is_group && name[0] == 'a' && name[1] == 'l' && name[2] == 'l') {
    end = name + 3;
    userid = 0;
    is_shared_gid = true;
  } else if (name[0] == 'u' && std::isdigit(static_cast<unsigned char>(name[1]))) {
    char* parsed;
    userid = std::strtoull(name + 1, &parsed, 10);
    end = parsed;
  } else {
    SetGuestErrno(guest::kENOENT);
    return 0;
  }
  if (end[0] != '_' || end[1] == 0) {
    SetGuestErrno(guest::kENOENT);
    return 0;
  }
  std::uint64_t appid = 0;
  if (end[1] == 'a' && std::isdigit(static_cast<unsigned char>(end[2]))) {
    char* parsed;
    if (is_shared_gid) {
      appid = std::strtoull(end + 2, &parsed, 10) + kAidSharedGidStart;
      end = parsed;
      if (appid > kAidSharedGidEnd) {
        SetGuestErrno(guest::kENOENT);
        return 0;
      }
    } else {
      appid = std::strtoull(end + 2, &parsed, 10);
      end = parsed;
      if (is_group) {
        if (std::strcmp(end, "_ext_cache") == 0) {
          end += 10;
          appid += kAidExtCacheGidStart;
        } else if (std::strcmp(end, "_ext") == 0) {
          end += 4;
          appid += kAidExtGidStart;
        } else if (std::strcmp(end, "_cache") == 0) {
          end += 6;
          appid += kAidCacheGidStart;
        } else {
          appid += kAidAppStart;
        }
      } else {
        appid += kAidAppStart;
      }
    }
  } else if (end[1] == 'i' && std::isdigit(static_cast<unsigned char>(end[2]))) {
    char* parsed;
    appid = std::strtoull(end + 2, &parsed, 10) + kAidIsolatedStart;
    end = parsed;
  } else if (const AndroidIdInfo* info = FindAndroidIdInfo(end + 1); info != nullptr) {
    appid = info->aid;
    end += std::strlen(info->name) + 1;
    if (!PlatformIdSecondaryUserAllowed(static_cast<std::uint32_t>(appid))) {
      SetGuestErrno(guest::kENOENT);
      return 0;
    }
  }
  if (end[0] != 0 || userid > 1000 || appid >= kAidUserOffset) {
    SetGuestErrno(guest::kENOENT);
    return 0;
  }
  return static_cast<std::uint32_t>(appid + userid * kAidUserOffset);
}

void AppNameFromUid(std::uint32_t uid, char* buffer, std::size_t length) noexcept {
  const std::uint32_t appid = uid % kAidUserOffset;
  const std::uint32_t userid = uid / kAidUserOffset;
  if (appid >= kAidIsolatedStart) {
    std::snprintf(buffer, length, "u%u_i%u", userid, appid - kAidIsolatedStart);
  } else if (appid < kAidAppStart) {
    if (const AndroidIdInfo* info = FindAndroidIdInfo(appid); info != nullptr) {
      std::snprintf(buffer, length, "u%u_%s", userid, info->name);
    }
  } else {
    std::snprintf(buffer, length, "u%u_a%u", userid, appid - kAidAppStart);
  }
}

void AppNameFromGid(std::uint32_t gid, char* buffer, std::size_t length) noexcept {
  const std::uint32_t appid = gid % kAidUserOffset;
  const std::uint32_t userid = gid / kAidUserOffset;
  if (appid >= kAidIsolatedStart) {
    std::snprintf(buffer, length, "u%u_i%u", userid, appid - kAidIsolatedStart);
  } else if (userid == 0 && appid >= kAidSharedGidStart && appid <= kAidSharedGidEnd) {
    std::snprintf(buffer, length, "all_a%u", appid - kAidSharedGidStart);
  } else if (appid >= kAidExtCacheGidStart && appid <= kAidExtCacheGidEnd) {
    std::snprintf(buffer, length, "u%u_a%u_ext_cache", userid, appid - kAidExtCacheGidStart);
  } else if (appid >= kAidExtGidStart && appid <= kAidExtGidEnd) {
    std::snprintf(buffer, length, "u%u_a%u_ext", userid, appid - kAidExtGidStart);
  } else if (appid >= kAidCacheGidStart && appid <= kAidCacheGidEnd) {
    std::snprintf(buffer, length, "u%u_a%u_cache", userid, appid - kAidCacheGidStart);
  } else if (appid < kAidAppStart) {
    if (const AndroidIdInfo* info = FindAndroidIdInfo(appid); info != nullptr) {
      std::snprintf(buffer, length, "u%u_%s", userid, info->name);
    }
  } else {
    std::snprintf(buffer, length, "u%u_a%u", userid, appid - kAidAppStart);
  }
}

bool DeviceLaunchedBeforeApi29() noexcept {
  try {
    const std::optional<std::string> value = properties::GetProperty("ro.product.first_api_level");
    if (!value.has_value() || value->empty()) return false;
    const int level = std::atoi(value->c_str());
    return level != 0 && level < 29;
  } catch (...) {
    return false;
  }
}

bool IsOemId(std::uint32_t id) noexcept {
  if (DeviceLaunchedBeforeApi29() && id >= kAidOemReservedStart && id < kAidEverybody &&
      FindAndroidIdInfo(id) == nullptr) {
    return true;
  }
  return (id >= kAidOemReservedStart && id <= kAidOemReservedEnd) ||
         (id >= kAidOemReserved2Start && id <= kAidOemReserved2End);
}

std::uint32_t OemIdFromName(const char* name) noexcept {
  unsigned id;
  if (std::sscanf(name, "oem_%u", &id) != 1) return 0;
  if (!IsOemId(id)) return 0;
  return id;
}

GuestPasswd* OemIdToPasswd(std::uint32_t uid, PasswdState* state) noexcept {
  if (!IsOemId(uid)) return nullptr;
  std::snprintf(state->name_buffer_, sizeof(state->name_buffer_), "oem_%u", uid);
  std::snprintf(state->dir_buffer_, sizeof(state->dir_buffer_), "/");
  std::snprintf(state->sh_buffer_, sizeof(state->sh_buffer_), "/bin/sh");
  GuestPasswd* pw = &state->passwd_;
  pw->pw_uid = uid;
  pw->pw_gid = uid;
  return pw;
}

GuestGroup* OemIdToGroup(std::uint32_t gid, GroupState* state) noexcept {
  if (!IsOemId(gid)) return nullptr;
  std::snprintf(state->group_name_buffer_, sizeof(state->group_name_buffer_), "oem_%u", gid);
  GuestGroup* gr = &state->group_;
  gr->gr_gid = gid;
  return gr;
}

GuestPasswd* AppIdToPasswd(std::uint32_t uid, PasswdState* state) noexcept {
  if (uid < kAidAppStart || !IsValidAppId(uid, false)) {
    SetGuestErrno(guest::kENOENT);
    return nullptr;
  }
  AppNameFromUid(uid, state->name_buffer_, sizeof(state->name_buffer_));
  const std::uint32_t appid = uid % kAidUserOffset;
  std::snprintf(state->dir_buffer_, sizeof(state->dir_buffer_), appid < kAidAppStart ? "/" : "/data");
  std::snprintf(state->sh_buffer_, sizeof(state->sh_buffer_), "/bin/sh");
  GuestPasswd* pw = &state->passwd_;
  pw->pw_uid = uid;
  pw->pw_gid = uid;
  return pw;
}

GuestGroup* AppIdToGroup(std::uint32_t gid, GroupState* state) noexcept {
  if (gid < kAidAppStart || !IsValidAppId(gid, true)) {
    SetGuestErrno(guest::kENOENT);
    return nullptr;
  }
  AppNameFromGid(gid, state->group_name_buffer_, sizeof(state->group_name_buffer_));
  GuestGroup* gr = &state->group_;
  gr->gr_gid = gid;
  return gr;
}

GuestPasswd* GetpwuidInternal(std::uint32_t uid, PasswdState* state) noexcept {
  if (const AndroidIdInfo* info = FindAndroidIdInfo(uid); info != nullptr) return IdInfoToPasswd(state, info);
  if (GuestPasswd* pw = OemIdToPasswd(uid, state); pw != nullptr) return pw;
  return AppIdToPasswd(uid, state);
}

GuestPasswd* GetpwnamInternal(const char* login, PasswdState* state) noexcept {
  if (const AndroidIdInfo* info = FindAndroidIdInfo(login); info != nullptr) return IdInfoToPasswd(state, info);
  if (GuestPasswd* pw = OemIdToPasswd(OemIdFromName(login), state); pw != nullptr) return pw;
  return AppIdToPasswd(AppIdFromName(login, false), state);
}

GuestGroup* GetgrgidInternal(std::uint32_t gid, GroupState* state) noexcept {
  if (const AndroidIdInfo* info = FindAndroidIdInfo(gid); info != nullptr) return IdInfoToGroup(state, info);
  if (GuestGroup* gr = OemIdToGroup(gid, state); gr != nullptr) return gr;
  return AppIdToGroup(gid, state);
}

GuestGroup* GetgrnamInternal(const char* name, GroupState* state) noexcept {
  if (const AndroidIdInfo* info = FindAndroidIdInfo(name); info != nullptr) return IdInfoToGroup(state, info);
  if (GuestGroup* gr = OemIdToGroup(OemIdFromName(name), state); gr != nullptr) return gr;
  return AppIdToGroup(AppIdFromName(name, true), state);
}

char* AlignedState(char* buffer) noexcept {
  const auto address = reinterpret_cast<std::uintptr_t>(buffer);
  return reinterpret_cast<char*>((address + sizeof(std::uintptr_t) - 1) & ~(sizeof(std::uintptr_t) - 1));
}

int GetPasswdReentrant(bool by_name, const char* name, std::uint32_t uid, GuestPasswd* pwd, char* buffer,
                       abi::g_size length, GuestPasswd** result) noexcept {
  const int saved = GuestErrno();
  if (result == nullptr) return guest::kEINVAL;
  *result = nullptr;
  if (pwd == nullptr || buffer == nullptr || (by_name && name == nullptr)) return guest::kEINVAL;
  char* p = AlignedState(buffer);
  if (static_cast<abi::g_size>(p - buffer) + sizeof(PasswdState) > length) return guest::kERANGE;
  auto* state = reinterpret_cast<PasswdState*>(p);
  InitPasswdState(state);
  GuestPasswd* found = by_name ? GetpwnamInternal(name, state) : GetpwuidInternal(uid, state);
  const int error = GuestErrno();
  SetGuestErrno(saved);
  if (found != nullptr) {
    *pwd = *found;
    *result = pwd;
    return 0;
  }
  return error;
}

int GetGroupReentrant(bool by_name, const char* name, std::uint32_t gid, GuestGroup* grp, char* buffer,
                      abi::g_size length, GuestGroup** result) noexcept {
  const int saved = GuestErrno();
  if (result == nullptr) return guest::kEINVAL;
  *result = nullptr;
  if (grp == nullptr || buffer == nullptr || (by_name && name == nullptr)) return guest::kEINVAL;
  char* p = AlignedState(buffer);
  if (static_cast<abi::g_size>(p - buffer) + sizeof(GroupState) > length) return guest::kERANGE;
  auto* state = reinterpret_cast<GroupState*>(p);
  InitGroupState(state);
  GuestGroup* found = by_name ? GetgrnamInternal(name, state) : GetgrgidInternal(gid, state);
  const int error = GuestErrno();
  SetGuestErrno(saved);
  if (found != nullptr) {
    *grp = *found;
    *result = grp;
    return 0;
  }
  return error;
}

}
}

using core::GuestGroup;
using core::GuestPasswd;

ELIXIR_BIONIC_FN(GuestPasswd*, getpwuid)(abi::g_uid uid) noexcept {
  return core::GetpwuidInternal(uid, core::PasswdTlsBuffer());
}

ELIXIR_BIONIC_FN(GuestPasswd*, getpwnam)(const char* login) noexcept {
  if (login == nullptr) return FailWith(guest::kENOENT, static_cast<GuestPasswd*>(nullptr));
  return core::GetpwnamInternal(login, core::PasswdTlsBuffer());
}

ELIXIR_BIONIC_FN(int, getpwnam_r)(const char* name, GuestPasswd* pwd, char* buffer, abi::g_size length,
                                  GuestPasswd** result) noexcept {
  return core::GetPasswdReentrant(true, name, static_cast<std::uint32_t>(-1), pwd, buffer, length, result);
}

ELIXIR_BIONIC_FN(int, getpwuid_r)(abi::g_uid uid, GuestPasswd* pwd, char* buffer, abi::g_size length,
                                  GuestPasswd** result) noexcept {
  return core::GetPasswdReentrant(false, nullptr, uid, pwd, buffer, length, result);
}

ELIXIR_BIONIC_FN(void, setpwent)() noexcept { core::PasswdTlsBuffer()->getpwent_idx = 0; }
ELIXIR_BIONIC_FN(void, endpwent)() noexcept { elixir_bionic_setpwent(); }

ELIXIR_BIONIC_FN(GuestPasswd*, getpwent)() noexcept {
  using namespace core;
  PasswdState* state = PasswdTlsBuffer();
  if (state->getpwent_idx < 0) return nullptr;
  abi::g_ssize start = 0;
  abi::g_ssize end = static_cast<abi::g_ssize>(kAndroidIdCount);
  if (state->getpwent_idx < end) return IdInfoToPasswd(state, kAndroidIds + state->getpwent_idx++);
  start = end;
  end += kAidOemReservedEnd - kAidOemReservedStart + 1;
  if (state->getpwent_idx < end) {
    return OemIdToPasswd(static_cast<std::uint32_t>(state->getpwent_idx++ - start + kAidOemReservedStart), state);
  }
  start = end;
  end += kAidOemReserved2End - kAidOemReserved2Start + 1;
  if (state->getpwent_idx < end) {
    return OemIdToPasswd(static_cast<std::uint32_t>(state->getpwent_idx++ - start + kAidOemReserved2Start), state);
  }
  start = end;
  end += kAidSystemExtReservedEnd - kAidSystemReservedStart + 1;
  if (state->getpwent_idx < end) {
    GuestPasswd* pw = nullptr;
    while (pw == nullptr && state->getpwent_idx < end) {
      pw = OemIdToPasswd(static_cast<std::uint32_t>(state->getpwent_idx++ - start + kAidSystemReservedStart), state);
    }
    if (pw != nullptr) return pw;
  }
  state->getpwent_idx = NextAppId(state->getpwent_idx, false);
  if (state->getpwent_idx != -1) return AppIdToPasswd(static_cast<std::uint32_t>(state->getpwent_idx), state);
  return nullptr;
}

ELIXIR_BIONIC_FN(GuestGroup*, getgrgid)(abi::g_gid gid) noexcept {
  return core::GetgrgidInternal(gid, core::GroupTlsBuffer());
}

ELIXIR_BIONIC_FN(GuestGroup*, getgrnam)(const char* name) noexcept {
  if (name == nullptr) return FailWith(guest::kENOENT, static_cast<GuestGroup*>(nullptr));
  return core::GetgrnamInternal(name, core::GroupTlsBuffer());
}

ELIXIR_BIONIC_FN(int, getgrgid_r)(abi::g_gid gid, GuestGroup* grp, char* buffer, abi::g_size length,
                                  GuestGroup** result) noexcept {
  return core::GetGroupReentrant(false, nullptr, gid, grp, buffer, length, result);
}

ELIXIR_BIONIC_FN(int, getgrnam_r)(const char* name, GuestGroup* grp, char* buffer, abi::g_size length,
                                  GuestGroup** result) noexcept {
  return core::GetGroupReentrant(true, name, 0, grp, buffer, length, result);
}

ELIXIR_BIONIC_FN(void, setgrent)() noexcept { core::GroupTlsBuffer()->getgrent_idx = 0; }
ELIXIR_BIONIC_FN(void, endgrent)() noexcept { elixir_bionic_setgrent(); }

ELIXIR_BIONIC_FN(GuestGroup*, getgrent)() noexcept {
  using namespace core;
  GroupState* state = GroupTlsBuffer();
  if (state->getgrent_idx < 0) return nullptr;
  abi::g_ssize start = 0;
  abi::g_ssize end = static_cast<abi::g_ssize>(kAndroidIdCount);
  if (state->getgrent_idx < end) return IdInfoToGroup(state, kAndroidIds + state->getgrent_idx++);
  start = end;
  end += kAidOemReservedEnd - kAidOemReservedStart + 1;
  if (state->getgrent_idx < end) {
    return OemIdToGroup(static_cast<std::uint32_t>(state->getgrent_idx++ - start + kAidOemReservedStart), state);
  }
  start = end;
  end += kAidOemReserved2End - kAidOemReserved2Start + 1;
  if (state->getgrent_idx < end) {
    return OemIdToGroup(static_cast<std::uint32_t>(state->getgrent_idx++ - start + kAidOemReserved2Start), state);
  }
  start = end;
  end += kAidSystemExtReservedEnd - kAidSystemReservedStart + 1;
  if (state->getgrent_idx < end) {
    InitGroupState(state);
    GuestGroup* gr = nullptr;
    while (gr == nullptr && state->getgrent_idx < end) {
      gr = OemIdToGroup(static_cast<std::uint32_t>(state->getgrent_idx++ - start + kAidSystemReservedStart), state);
    }
    if (gr != nullptr) return gr;
  }
  state->getgrent_idx = NextAppId(state->getgrent_idx, true);
  if (state->getgrent_idx != -1) return AppIdToGroup(static_cast<std::uint32_t>(state->getgrent_idx), state);
  return nullptr;
}

ELIXIR_BIONIC_FN(int, getgrouplist)(const char* user, abi::g_gid group, abi::g_gid* groups, int* ngroups) noexcept {
  (void)user;
  if (ngroups == nullptr) return FailWith(guest::kEFAULT, -1);
  if (*ngroups < 1) {
    *ngroups = 1;
    return -1;
  }
  if (groups == nullptr) return FailWith(guest::kEFAULT, -1);
  groups[0] = group;
  return (*ngroups = 1);
}

ELIXIR_BIONIC_FN(int, initgroups)(const char* user, abi::g_gid group) noexcept {
  (void)user;
  const abi::g_gid groups[] = {group};
  return elixir_bionic_setgroups(1, groups);
}

ELIXIR_BIONIC_FN(char*, getlogin)() noexcept {
  GuestPasswd* pw = elixir_bionic_getpwuid(core::CurrentUid());
  return pw != nullptr ? pw->pw_name : nullptr;
}

ELIXIR_BIONIC_FN(int, getlogin_r)(char* buffer, abi::g_size size) noexcept {
  const char* login = elixir_bionic_getlogin();
  if (login == nullptr) return GuestErrno();
  const std::size_t length = std::strlen(login) + 1;
  if (length > size) return guest::kERANGE;
  if (buffer == nullptr) return guest::kEFAULT;
  std::memcpy(buffer, login, length);
  return 0;
}

namespace core {

void RegisterIdentityExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(getpwuid);
  ELIXIR_ADD(getpwnam);
  ELIXIR_ADD(getpwuid_r);
  ELIXIR_ADD(getpwnam_r);
  ELIXIR_ADD(getpwent);
  ELIXIR_ADD(setpwent);
  ELIXIR_ADD(endpwent);
  ELIXIR_ADD(getgrgid);
  ELIXIR_ADD(getgrnam);
  ELIXIR_ADD(getgrgid_r);
  ELIXIR_ADD(getgrnam_r);
  ELIXIR_ADD(getgrent);
  ELIXIR_ADD(setgrent);
  ELIXIR_ADD(endgrent);
  ELIXIR_ADD(getgrouplist);
  ELIXIR_ADD(initgroups);
  ELIXIR_ADD(getlogin);
  ELIXIR_ADD(getlogin_r);
#undef ELIXIR_ADD
}

}
}
