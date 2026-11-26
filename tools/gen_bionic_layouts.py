#!/usr/bin/env python3
# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
import argparse
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_bionic_constants import elf_symbols, find_ndk  # noqa: E402

HEADERS = [
    "sys/stat.h", "sys/statfs.h", "sys/statvfs.h", "time.h", "sys/time.h", "dirent.h",
    "netdb.h", "sys/socket.h", "sys/un.h", "netinet/in.h", "poll.h", "sys/epoll.h",
    "signal.h", "ucontext.h", "pthread.h", "semaphore.h", "sys/resource.h",
    "sys/utsname.h", "locale.h", "dlfcn.h", "link.h", "setjmp.h", "stdio.h",
    "sys/uio.h", "sys/times.h", "sys/sysinfo.h", "wchar.h", "fenv.h", "sched.h",
    "net/if.h", "ifaddrs.h", "termios.h", "sys/select.h", "stddef.h", "stdlib.h",
]

STRUCTS = [
    ("struct stat", "Stat", ["st_dev", "st_ino", "st_nlink", "st_mode", "st_uid", "st_gid",
                             "st_rdev", "st_size", "st_blksize", "st_blocks", "st_atim",
                             "st_mtim", "st_ctim"]),
    ("struct statfs", "Statfs", ["f_type", "f_bsize", "f_blocks", "f_bfree", "f_bavail",
                                 "f_files", "f_ffree", "f_fsid", "f_namelen", "f_frsize",
                                 "f_flags"]),
    ("struct statvfs", "Statvfs", ["f_bsize", "f_frsize", "f_blocks", "f_bfree", "f_bavail",
                                   "f_files", "f_ffree", "f_favail", "f_fsid", "f_flag",
                                   "f_namemax"]),
    ("struct timespec", "Timespec", ["tv_sec", "tv_nsec"]),
    ("struct timeval", "Timeval", ["tv_sec", "tv_usec"]),
    ("struct timezone", "Timezone", ["tz_minuteswest", "tz_dsttime"]),
    ("struct itimerval", "Itimerval", ["it_interval", "it_value"]),
    ("struct tm", "Tm", ["tm_sec", "tm_min", "tm_hour", "tm_mday", "tm_mon", "tm_year",
                         "tm_wday", "tm_yday", "tm_isdst", "tm_gmtoff", "tm_zone"]),
    ("struct tms", "Tms", ["tms_utime", "tms_stime", "tms_cutime", "tms_cstime"]),
    ("struct dirent", "Dirent", ["d_ino", "d_off", "d_reclen", "d_type", "d_name"]),
    ("struct addrinfo", "Addrinfo", ["ai_flags", "ai_family", "ai_socktype", "ai_protocol",
                                     "ai_addrlen", "ai_canonname", "ai_addr", "ai_next"]),
    ("struct hostent", "Hostent", ["h_name", "h_aliases", "h_addrtype", "h_length",
                                   "h_addr_list"]),
    ("struct sockaddr", "Sockaddr", ["sa_family", "sa_data"]),
    ("struct sockaddr_in", "SockaddrIn", ["sin_family", "sin_port", "sin_addr"]),
    ("struct sockaddr_in6", "SockaddrIn6", ["sin6_family", "sin6_port", "sin6_flowinfo",
                                            "sin6_addr", "sin6_scope_id"]),
    ("struct sockaddr_un", "SockaddrUn", ["sun_family", "sun_path"]),
    ("struct sockaddr_storage", "SockaddrStorage", ["ss_family"]),
    ("struct msghdr", "Msghdr", ["msg_name", "msg_namelen", "msg_iov", "msg_iovlen",
                                 "msg_control", "msg_controllen", "msg_flags"]),
    ("struct cmsghdr", "Cmsghdr", ["cmsg_len", "cmsg_level", "cmsg_type"]),
    ("struct iovec", "Iovec", ["iov_base", "iov_len"]),
    ("struct linger", "Linger", ["l_onoff", "l_linger"]),
    ("struct pollfd", "Pollfd", ["fd", "events", "revents"]),
    ("struct epoll_event", "EpollEvent", ["events", "data"]),
    ("struct sigaction", "Sigaction", ["sa_flags", "sa_handler", "sa_mask", "sa_restorer"]),
    ("sigset_t", "Sigset", []),
    ("siginfo_t", "Siginfo", ["si_signo", "si_errno", "si_code"]),
    ("stack_t", "StackT", ["ss_sp", "ss_flags", "ss_size"]),
    ("ucontext_t", "Ucontext", ["uc_flags", "uc_link", "uc_stack", "uc_mcontext",
                                "uc_sigmask"]),
    ("mcontext_t", "Mcontext", ["gregs", "fpregs"]),
    ("pthread_attr_t", "PthreadAttr", ["flags", "stack_base", "stack_size", "guard_size",
                                       "sched_policy", "sched_priority"]),
    ("pthread_mutex_t", "PthreadMutex", []),
    ("pthread_mutexattr_t", "PthreadMutexattr", []),
    ("pthread_cond_t", "PthreadCond", []),
    ("pthread_condattr_t", "PthreadCondattr", []),
    ("pthread_rwlock_t", "PthreadRwlock", []),
    ("pthread_rwlockattr_t", "PthreadRwlockattr", []),
    ("pthread_barrier_t", "PthreadBarrier", []),
    ("pthread_spinlock_t", "PthreadSpinlock", []),
    ("pthread_key_t", "PthreadKey", []),
    ("pthread_once_t", "PthreadOnce", []),
    ("pthread_t", "PthreadT", []),
    ("sem_t", "Sem", []),
    ("struct sched_param", "SchedParam", ["sched_priority"]),
    ("struct rlimit", "Rlimit", ["rlim_cur", "rlim_max"]),
    ("struct rusage", "Rusage", ["ru_utime", "ru_stime", "ru_maxrss"]),
    ("struct utsname", "Utsname", ["sysname", "nodename", "release", "version", "machine",
                                   "domainname"]),
    ("struct sysinfo", "Sysinfo", ["uptime", "loads", "totalram", "freeram", "sharedram",
                                   "bufferram", "totalswap", "freeswap", "procs",
                                   "totalhigh", "freehigh", "mem_unit"]),
    ("struct lconv", "Lconv", ["decimal_point", "thousands_sep", "grouping",
                               "int_curr_symbol", "currency_symbol"]),
    ("Dl_info", "DlInfo", ["dli_fname", "dli_fbase", "dli_sname", "dli_saddr"]),
    ("struct dl_phdr_info", "DlPhdrInfo", ["dlpi_addr", "dlpi_name", "dlpi_phdr",
                                           "dlpi_phnum", "dlpi_adds", "dlpi_subs",
                                           "dlpi_tls_modid", "dlpi_tls_data"]),
    ("jmp_buf", "JmpBuf", []),
    ("sigjmp_buf", "SigjmpBuf", []),
    ("fpos_t", "FposT", []),
    ("mbstate_t", "Mbstate", []),
    ("wchar_t", "Wchar", []),
    ("wint_t", "Wint", []),
    ("long", "Long", []),
    ("long double", "LongDouble", []),
    ("max_align_t", "MaxAlign", []),
    ("off_t", "Off", []),
    ("time_t", "Time", []),
    ("clock_t", "Clock", []),
    ("size_t", "Size", []),
    ("socklen_t", "Socklen", []),
    ("fenv_t", "Fenv", []),
    ("cpu_set_t", "CpuSet", []),
    ("fd_set", "FdSet", []),
    ("struct ifaddrs", "Ifaddrs", ["ifa_next", "ifa_name", "ifa_flags", "ifa_addr",
                                   "ifa_netmask", "ifa_dstaddr", "ifa_data"]),
    ("struct termios", "Termios", ["c_iflag", "c_oflag", "c_cflag", "c_lflag", "c_line",
                                   "c_cc"]),
    ("struct winsize", "Winsize", ["ws_row", "ws_col", "ws_xpixel", "ws_ypixel"]),
    ("div_t", "DivT", ["quot", "rem"]),
    ("ldiv_t", "LdivT", ["quot", "rem"]),
    ("lldiv_t", "LldivT", ["quot", "rem"]),
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ndk")
    parser.add_argument("--api", default="31")
    parser.add_argument("--out", default=str(pathlib.Path(__file__).resolve().parent.parent
                                             / "src" / "bionic" / "guest_layout.h"))
    args = parser.parse_args()
    ndk = find_ndk(args.ndk)
    clang = ndk / "toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe"
    work = pathlib.Path(tempfile.mkdtemp(prefix="elixir_layout_"))
    src = ["#define _GNU_SOURCE 1"] + [f"#include <{h}>" for h in HEADERS]
    keys = []
    for ctype, name, fields in STRUCTS:
        src.append(f"__attribute__((used)) const long long elixir_l_{name}__size = sizeof({ctype});")
        src.append(f"__attribute__((used)) const long long elixir_l_{name}__align = _Alignof({ctype});")
        keys += [f"{name}__size", f"{name}__align"]
        for field in fields:
            src.append(f"__attribute__((used)) const long long elixir_l_{name}_{field} = "
                       f"__builtin_offsetof({ctype}, {field});")
            keys.append(f"{name}_{field}")
    (work / "layout.c").write_text("\n".join(src) + "\n")
    subprocess.run([str(clang), f"--target=x86_64-linux-android{args.api}", "-c", "-O0", "-w",
                    "-o", str(work / "layout.o"), str(work / "layout.c")], check=True)
    data = (work / "layout.o").read_bytes()
    symbols, sections = elf_symbols(data)
    lines = [
        "// Copyright 2026 Elixir Project Authors",
        "// Licensed under the Apache License, Version 2.0",
        "//",
        "// GENERATED by tools/gen_bionic_layouts.py from the Android NDK's x86_64",
        f"// bionic headers (API {args.api}). Do not edit by hand.",
        "#ifndef ELIXIR_BIONIC_GUEST_LAYOUT_H_",
        "#define ELIXIR_BIONIC_GUEST_LAYOUT_H_",
        "",
        "#include <cstddef>",
        "",
        "namespace elixir::bionic::layout {",
        "",
    ]
    for key in keys:
        shndx, value, _ = symbols[f"elixir_l_{key}"]
        number = struct.unpack_from("<q", data, sections[shndx][2] + value)[0]
        ident = re.sub(r"[^A-Za-z0-9_]", "_", key)
        lines.append(f"inline constexpr std::size_t k{ident} = {number};")
    lines += ["", "}", "", "#endif", ""]
    out = pathlib.Path(args.out)
    out.write_text("\n".join(lines), newline="\n")
    print(f"wrote {len(keys)} layout facts to {out}")


if __name__ == "__main__":
    main()
