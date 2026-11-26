// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_GUEST_TYPES_H_
#define ELIXIR_BIONIC_GUEST_TYPES_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_abi.h"
#include "bionic/guest_layout.h"

namespace elixir::bionic::guest {

using abi::g_long;
using abi::g_ulong;
using abi::g_size;
using abi::g_ssize;
using abi::g_off;
using abi::g_time;
using abi::g_wchar;
using abi::g_wint;

struct Timespec {
  g_time tv_sec;
  g_long tv_nsec;
};
struct Timeval {
  g_time tv_sec;
  g_long tv_usec;
};
struct Timezone {
  std::int32_t tz_minuteswest;
  std::int32_t tz_dsttime;
};
struct Itimerval {
  Timeval it_interval;
  Timeval it_value;
};

struct Tm {
  std::int32_t tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
  g_long tm_gmtoff;
  const char* tm_zone;
};

struct Tms {
  g_long tms_utime, tms_stime, tms_cutime, tms_cstime;
};

struct Stat {
  std::uint64_t st_dev;
  std::uint64_t st_ino;
  std::uint64_t st_nlink;
  std::uint32_t st_mode;
  std::uint32_t st_uid;
  std::uint32_t st_gid;
  std::uint32_t pad0;
  std::uint64_t st_rdev;
  std::int64_t st_size;
  std::int64_t st_blksize;
  std::int64_t st_blocks;
  Timespec st_atim;
  Timespec st_mtim;
  Timespec st_ctim;
  std::int64_t reserved[3];
};

struct Statfs {
  std::uint64_t f_type, f_bsize, f_blocks, f_bfree, f_bavail, f_files, f_ffree;
  std::int32_t f_fsid[2];
  std::uint64_t f_namelen, f_frsize, f_flags;
  std::uint64_t f_spare[4];
};

struct Statvfs {
  std::uint64_t f_bsize, f_frsize, f_blocks, f_bfree, f_bavail, f_files, f_ffree, f_favail, f_fsid,
      f_flag, f_namemax;
  std::uint32_t reserved[6];
};

struct Dirent {
  std::uint64_t d_ino;
  std::int64_t d_off;
  std::uint16_t d_reclen;
  std::uint8_t d_type;
  char d_name[256];
};

struct Sockaddr {
  std::uint16_t sa_family;
  char sa_data[14];
};
struct InAddr {
  std::uint32_t s_addr;
};
struct In6Addr {
  std::uint8_t s6_addr[16];
};
struct SockaddrIn {
  std::uint16_t sin_family;
  std::uint16_t sin_port;
  InAddr sin_addr;
  std::uint8_t sin_zero[8];
};
struct SockaddrIn6 {
  std::uint16_t sin6_family;
  std::uint16_t sin6_port;
  std::uint32_t sin6_flowinfo;
  In6Addr sin6_addr;
  std::uint32_t sin6_scope_id;
};
struct SockaddrUn {
  std::uint16_t sun_family;
  char sun_path[108];
};
struct alignas(8) SockaddrStorage {
  std::uint16_t ss_family;
  char data[126];
};

struct Addrinfo {
  std::int32_t ai_flags;
  std::int32_t ai_family;
  std::int32_t ai_socktype;
  std::int32_t ai_protocol;
  std::uint32_t ai_addrlen;
  char* ai_canonname;
  Sockaddr* ai_addr;
  Addrinfo* ai_next;
};

struct Hostent {
  char* h_name;
  char** h_aliases;
  std::int32_t h_addrtype;
  std::int32_t h_length;
  char** h_addr_list;
};

struct Iovec {
  void* iov_base;
  g_size iov_len;
};

struct Msghdr {
  void* msg_name;
  std::uint32_t msg_namelen;
  Iovec* msg_iov;
  g_size msg_iovlen;
  void* msg_control;
  g_size msg_controllen;
  std::int32_t msg_flags;
};

struct Cmsghdr {
  g_size cmsg_len;
  std::int32_t cmsg_level;
  std::int32_t cmsg_type;
};

struct Linger {
  std::int32_t l_onoff;
  std::int32_t l_linger;
};

struct Pollfd {
  std::int32_t fd;
  std::int16_t events;
  std::int16_t revents;
};

#pragma pack(push, 1)
struct EpollEvent {
  std::uint32_t events;
  std::uint64_t data;
};
#pragma pack(pop)

using Sigset = std::uint64_t;

struct Sigaction {
  std::int32_t sa_flags;
  void* sa_handler;
  Sigset sa_mask;
  void* sa_restorer;
};

struct StackT {
  void* ss_sp;
  std::int32_t ss_flags;
  g_size ss_size;
};

struct Siginfo {
  std::int32_t si_signo;
  std::int32_t si_errno;
  std::int32_t si_code;
  std::int32_t pad;
  union {
    std::int32_t raw[28];
    struct {
      void* si_addr;
    } fault;
    struct {
      std::int32_t si_pid;
      std::uint32_t si_uid;
    } kill;
  } fields;
};

inline constexpr int kRegR8 = 0, kRegR9 = 1, kRegR10 = 2, kRegR11 = 3, kRegR12 = 4, kRegR13 = 5,
                     kRegR14 = 6, kRegR15 = 7, kRegRdi = 8, kRegRsi = 9, kRegRbp = 10, kRegRbx = 11,
                     kRegRdx = 12, kRegRax = 13, kRegRcx = 14, kRegRsp = 15, kRegRip = 16,
                     kRegEfl = 17, kRegCsgsfs = 18, kRegErr = 19, kRegTrapno = 20, kRegOldmask = 21,
                     kRegCr2 = 22, kRegCount = 23;

struct Mcontext {
  std::int64_t gregs[kRegCount];
  void* fpregs;
  std::uint64_t reserved[8];
};

struct Ucontext {
  std::uint64_t uc_flags;
  Ucontext* uc_link;
  StackT uc_stack;
  Mcontext uc_mcontext;
  Sigset uc_sigmask;
  std::uint8_t fpregs_mem[512];
};

struct PthreadAttr {
  std::uint32_t flags;
  void* stack_base;
  g_size stack_size;
  g_size guard_size;
  std::int32_t sched_policy;
  std::int32_t sched_priority;
  char reserved[16];
};
inline constexpr std::uint32_t kPthreadAttrFlagDetached = 0x00000001;
inline constexpr std::uint32_t kPthreadAttrFlagUserAllocatedStack = 0x00000002;
inline constexpr std::uint32_t kPthreadAttrFlagInheritSched = 0x00000004;
inline constexpr std::uint32_t kPthreadAttrFlagExplicitSched = 0x00000008;

struct alignas(4) PthreadMutex {
  std::int32_t storage[10];
};
struct alignas(4) PthreadCond {
  std::int32_t storage[12];
};
struct alignas(4) PthreadRwlock {
  std::int32_t storage[14];
};
struct alignas(8) PthreadBarrier {
  std::int64_t storage[4];
};
using PthreadMutexattr = g_long;
using PthreadCondattr = g_long;
using PthreadRwlockattr = g_long;
using PthreadKey = std::int32_t;
using PthreadOnce = std::int32_t;
using PthreadT = g_long;
using PthreadSpinlock = std::int64_t;

struct alignas(4) Sem {
  std::uint32_t count;
  std::int32_t reserved[3];
};

struct SchedParam {
  std::int32_t sched_priority;
};

struct Rlimit {
  std::uint64_t rlim_cur;
  std::uint64_t rlim_max;
};

struct Rusage {
  Timeval ru_utime;
  Timeval ru_stime;
  g_long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss, ru_minflt, ru_majflt, ru_nswap, ru_inblock,
      ru_oublock, ru_msgsnd, ru_msgrcv, ru_nsignals, ru_nvcsw, ru_nivcsw;
};

struct Utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

struct Sysinfo {
  g_long uptime;
  g_ulong loads[3];
  g_ulong totalram, freeram, sharedram, bufferram, totalswap, freeswap;
  std::uint16_t procs;
  std::uint16_t pad;
  g_ulong totalhigh, freehigh;
  std::uint32_t mem_unit;
  char padding[4];
};

struct Lconv {
  char* decimal_point;
  char* thousands_sep;
  char* grouping;
  char* int_curr_symbol;
  char* currency_symbol;
  char* mon_decimal_point;
  char* mon_thousands_sep;
  char* mon_grouping;
  char* positive_sign;
  char* negative_sign;
  char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space, n_cs_precedes, n_sep_by_space,
      p_sign_posn, n_sign_posn, int_p_cs_precedes, int_p_sep_by_space, int_n_cs_precedes,
      int_n_sep_by_space, int_p_sign_posn, int_n_sign_posn;
};

struct DlInfo {
  const char* dli_fname;
  void* dli_fbase;
  const char* dli_sname;
  void* dli_saddr;
};

struct DlPhdrInfo {
  std::uint64_t dlpi_addr;
  const char* dlpi_name;
  const void* dlpi_phdr;
  std::uint16_t dlpi_phnum;
  unsigned long long dlpi_adds;
  unsigned long long dlpi_subs;
  g_size dlpi_tls_modid;
  void* dlpi_tls_data;
};

struct JmpBuf {
  g_long words[11];
};
struct SigjmpBuf {
  g_long words[12];
};

inline constexpr std::size_t kFileSize = 152;
struct alignas(8) File {
  std::uint8_t opaque[kFileSize];
};

using Mbstate = std::uint8_t[8];

struct Termios {
  std::uint32_t c_iflag, c_oflag, c_cflag, c_lflag;
  std::uint8_t c_line;
  std::uint8_t c_cc[19];
};
struct Winsize {
  std::uint16_t ws_row, ws_col, ws_xpixel, ws_ypixel;
};

struct DivT {
  std::int32_t quot, rem;
};
struct LdivT {
  g_long quot, rem;
};
struct LldivT {
  std::int64_t quot, rem;
};

#define ELIXIR_GUEST_LAYOUT_SIZE(type, name)                                   \
  static_assert(sizeof(type) == layout::k##name##__size, #type " size");      \
  static_assert(alignof(type) == layout::k##name##__align, #type " alignment")
#define ELIXIR_GUEST_LAYOUT_FIELD(type, name, field)                           \
  static_assert(offsetof(type, field) == layout::k##name##_##field, #type "::" #field)

ELIXIR_GUEST_LAYOUT_SIZE(Timespec, Timespec);
ELIXIR_GUEST_LAYOUT_FIELD(Timespec, Timespec, tv_nsec);
ELIXIR_GUEST_LAYOUT_SIZE(Timeval, Timeval);
ELIXIR_GUEST_LAYOUT_SIZE(Timezone, Timezone);
ELIXIR_GUEST_LAYOUT_SIZE(Itimerval, Itimerval);
ELIXIR_GUEST_LAYOUT_SIZE(Tm, Tm);
ELIXIR_GUEST_LAYOUT_FIELD(Tm, Tm, tm_isdst);
ELIXIR_GUEST_LAYOUT_FIELD(Tm, Tm, tm_gmtoff);
ELIXIR_GUEST_LAYOUT_FIELD(Tm, Tm, tm_zone);
ELIXIR_GUEST_LAYOUT_SIZE(Tms, Tms);
ELIXIR_GUEST_LAYOUT_SIZE(Stat, Stat);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_nlink);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_mode);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_rdev);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_size);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_blocks);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_atim);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_mtim);
ELIXIR_GUEST_LAYOUT_FIELD(Stat, Stat, st_ctim);
ELIXIR_GUEST_LAYOUT_SIZE(Statfs, Statfs);
ELIXIR_GUEST_LAYOUT_FIELD(Statfs, Statfs, f_fsid);
ELIXIR_GUEST_LAYOUT_FIELD(Statfs, Statfs, f_flags);
ELIXIR_GUEST_LAYOUT_SIZE(Statvfs, Statvfs);
ELIXIR_GUEST_LAYOUT_FIELD(Statvfs, Statvfs, f_namemax);
ELIXIR_GUEST_LAYOUT_SIZE(Dirent, Dirent);
ELIXIR_GUEST_LAYOUT_FIELD(Dirent, Dirent, d_reclen);
ELIXIR_GUEST_LAYOUT_FIELD(Dirent, Dirent, d_type);
ELIXIR_GUEST_LAYOUT_FIELD(Dirent, Dirent, d_name);
ELIXIR_GUEST_LAYOUT_SIZE(Sockaddr, Sockaddr);
ELIXIR_GUEST_LAYOUT_SIZE(SockaddrIn, SockaddrIn);
ELIXIR_GUEST_LAYOUT_FIELD(SockaddrIn, SockaddrIn, sin_addr);
ELIXIR_GUEST_LAYOUT_SIZE(SockaddrIn6, SockaddrIn6);
ELIXIR_GUEST_LAYOUT_FIELD(SockaddrIn6, SockaddrIn6, sin6_addr);
ELIXIR_GUEST_LAYOUT_FIELD(SockaddrIn6, SockaddrIn6, sin6_scope_id);
ELIXIR_GUEST_LAYOUT_SIZE(SockaddrUn, SockaddrUn);
ELIXIR_GUEST_LAYOUT_SIZE(SockaddrStorage, SockaddrStorage);
ELIXIR_GUEST_LAYOUT_SIZE(Addrinfo, Addrinfo);
ELIXIR_GUEST_LAYOUT_FIELD(Addrinfo, Addrinfo, ai_addrlen);
ELIXIR_GUEST_LAYOUT_FIELD(Addrinfo, Addrinfo, ai_canonname);
ELIXIR_GUEST_LAYOUT_FIELD(Addrinfo, Addrinfo, ai_addr);
ELIXIR_GUEST_LAYOUT_FIELD(Addrinfo, Addrinfo, ai_next);
ELIXIR_GUEST_LAYOUT_SIZE(Hostent, Hostent);
ELIXIR_GUEST_LAYOUT_FIELD(Hostent, Hostent, h_addr_list);
ELIXIR_GUEST_LAYOUT_SIZE(Iovec, Iovec);
ELIXIR_GUEST_LAYOUT_SIZE(Msghdr, Msghdr);
ELIXIR_GUEST_LAYOUT_FIELD(Msghdr, Msghdr, msg_iov);
ELIXIR_GUEST_LAYOUT_FIELD(Msghdr, Msghdr, msg_iovlen);
ELIXIR_GUEST_LAYOUT_FIELD(Msghdr, Msghdr, msg_control);
ELIXIR_GUEST_LAYOUT_FIELD(Msghdr, Msghdr, msg_controllen);
ELIXIR_GUEST_LAYOUT_FIELD(Msghdr, Msghdr, msg_flags);
ELIXIR_GUEST_LAYOUT_SIZE(Cmsghdr, Cmsghdr);
ELIXIR_GUEST_LAYOUT_SIZE(Linger, Linger);
ELIXIR_GUEST_LAYOUT_SIZE(Pollfd, Pollfd);
ELIXIR_GUEST_LAYOUT_SIZE(EpollEvent, EpollEvent);
ELIXIR_GUEST_LAYOUT_FIELD(EpollEvent, EpollEvent, data);
ELIXIR_GUEST_LAYOUT_SIZE(Sigaction, Sigaction);
ELIXIR_GUEST_LAYOUT_FIELD(Sigaction, Sigaction, sa_handler);
ELIXIR_GUEST_LAYOUT_FIELD(Sigaction, Sigaction, sa_mask);
ELIXIR_GUEST_LAYOUT_FIELD(Sigaction, Sigaction, sa_restorer);
static_assert(sizeof(Sigset) == layout::kSigset__size, "sigset_t size");
ELIXIR_GUEST_LAYOUT_SIZE(StackT, StackT);
ELIXIR_GUEST_LAYOUT_FIELD(StackT, StackT, ss_size);
static_assert(sizeof(Siginfo) == layout::kSiginfo__size, "siginfo_t size");
ELIXIR_GUEST_LAYOUT_SIZE(Mcontext, Mcontext);
ELIXIR_GUEST_LAYOUT_FIELD(Mcontext, Mcontext, fpregs);
ELIXIR_GUEST_LAYOUT_SIZE(Ucontext, Ucontext);
ELIXIR_GUEST_LAYOUT_FIELD(Ucontext, Ucontext, uc_mcontext);
ELIXIR_GUEST_LAYOUT_FIELD(Ucontext, Ucontext, uc_sigmask);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadAttr, PthreadAttr);
ELIXIR_GUEST_LAYOUT_FIELD(PthreadAttr, PthreadAttr, stack_base);
ELIXIR_GUEST_LAYOUT_FIELD(PthreadAttr, PthreadAttr, stack_size);
ELIXIR_GUEST_LAYOUT_FIELD(PthreadAttr, PthreadAttr, guard_size);
ELIXIR_GUEST_LAYOUT_FIELD(PthreadAttr, PthreadAttr, sched_policy);
ELIXIR_GUEST_LAYOUT_FIELD(PthreadAttr, PthreadAttr, sched_priority);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadMutex, PthreadMutex);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadCond, PthreadCond);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadRwlock, PthreadRwlock);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadBarrier, PthreadBarrier);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadMutexattr, PthreadMutexattr);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadCondattr, PthreadCondattr);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadRwlockattr, PthreadRwlockattr);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadKey, PthreadKey);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadOnce, PthreadOnce);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadT, PthreadT);
ELIXIR_GUEST_LAYOUT_SIZE(PthreadSpinlock, PthreadSpinlock);
ELIXIR_GUEST_LAYOUT_SIZE(Sem, Sem);
ELIXIR_GUEST_LAYOUT_SIZE(SchedParam, SchedParam);
ELIXIR_GUEST_LAYOUT_SIZE(Rlimit, Rlimit);
ELIXIR_GUEST_LAYOUT_SIZE(Rusage, Rusage);
ELIXIR_GUEST_LAYOUT_FIELD(Rusage, Rusage, ru_maxrss);
ELIXIR_GUEST_LAYOUT_SIZE(Utsname, Utsname);
ELIXIR_GUEST_LAYOUT_FIELD(Utsname, Utsname, machine);
ELIXIR_GUEST_LAYOUT_SIZE(Sysinfo, Sysinfo);
ELIXIR_GUEST_LAYOUT_FIELD(Sysinfo, Sysinfo, procs);
ELIXIR_GUEST_LAYOUT_FIELD(Sysinfo, Sysinfo, totalhigh);
ELIXIR_GUEST_LAYOUT_FIELD(Sysinfo, Sysinfo, mem_unit);
ELIXIR_GUEST_LAYOUT_SIZE(Lconv, Lconv);
ELIXIR_GUEST_LAYOUT_FIELD(Lconv, Lconv, currency_symbol);
ELIXIR_GUEST_LAYOUT_SIZE(DlInfo, DlInfo);
ELIXIR_GUEST_LAYOUT_SIZE(DlPhdrInfo, DlPhdrInfo);
ELIXIR_GUEST_LAYOUT_FIELD(DlPhdrInfo, DlPhdrInfo, dlpi_phnum);
ELIXIR_GUEST_LAYOUT_FIELD(DlPhdrInfo, DlPhdrInfo, dlpi_adds);
ELIXIR_GUEST_LAYOUT_FIELD(DlPhdrInfo, DlPhdrInfo, dlpi_tls_modid);
ELIXIR_GUEST_LAYOUT_FIELD(DlPhdrInfo, DlPhdrInfo, dlpi_tls_data);
ELIXIR_GUEST_LAYOUT_SIZE(JmpBuf, JmpBuf);
ELIXIR_GUEST_LAYOUT_SIZE(SigjmpBuf, SigjmpBuf);
static_assert(sizeof(Mbstate) == layout::kMbstate__size, "mbstate_t size");
static_assert(sizeof(g_wchar) == layout::kWchar__size, "wchar_t size");
static_assert(sizeof(abi::GuestLongDouble) == layout::kLongDouble__size, "long double size");
static_assert(alignof(abi::GuestLongDouble) == layout::kLongDouble__align, "long double alignment");
static_assert(sizeof(g_long) == layout::kLong__size, "long size");
static_assert(sizeof(g_off) == layout::kOff__size, "off_t size");
static_assert(sizeof(g_time) == layout::kTime__size, "time_t size");
ELIXIR_GUEST_LAYOUT_SIZE(Termios, Termios);
ELIXIR_GUEST_LAYOUT_FIELD(Termios, Termios, c_cc);
ELIXIR_GUEST_LAYOUT_SIZE(Winsize, Winsize);
ELIXIR_GUEST_LAYOUT_SIZE(DivT, DivT);
ELIXIR_GUEST_LAYOUT_SIZE(LdivT, LdivT);
ELIXIR_GUEST_LAYOUT_SIZE(LldivT, LldivT);

}

#endif
