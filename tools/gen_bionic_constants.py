#!/usr/bin/env python3
# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
import argparse
import os
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

HEADERS = [
    "errno.h", "fcntl.h", "sys/stat.h", "unistd.h", "signal.h", "sys/mman.h",
    "sys/socket.h", "sys/un.h", "netinet/in.h", "netinet/tcp.h", "netdb.h",
    "poll.h", "sys/epoll.h", "sys/eventfd.h", "time.h", "dlfcn.h", "pthread.h",
    "dirent.h", "sys/ioctl.h", "locale.h", "stdio.h", "sys/resource.h",
    "sys/prctl.h", "sched.h", "sys/wait.h", "sys/utsname.h", "termios.h",
    "net/if.h", "sys/statvfs.h", "sys/select.h", "sys/time.h", "limits.h",
    "android/log.h", "sys/file.h", "stdio_ext.h", "wchar.h",
]

FAMILIES = [
    r"E[A-Z0-9]+",
    r"O_[A-Z]+", r"AT_[A-Z_]+", r"F_[A-Z_0-9]+", r"FD_CLOEXEC", r"S_I[A-Z]+",
    r"SEEK_[A-Z]+", r"[RWXF]_OK",
    r"PROT_[A-Z_]+", r"MAP_[A-Z_0-9]+", r"MADV_[A-Z_]+", r"MS_(ASYNC|SYNC|INVALIDATE)",
    r"SIG[A-Z0-9]+", r"SIG_(BLOCK|UNBLOCK|SETMASK)", r"SEM_VALUE_MAX", r"SA_[A-Z]+", r"SI_[A-Z]+", r"SEGV_[A-Z]+", r"BUS_[A-Z]+",
    r"ILL_[A-Z]+", r"FPE_[A-Z]+", r"SS_[A-Z]+", r"MINSIGSTKSZ", r"SIGSTKSZ", r"NSIG",
    r"AF_[A-Z0-9]+", r"PF_[A-Z0-9]+", r"SOCK_[A-Z]+", r"SOL_[A-Z]+", r"SO_[A-Z_]+",
    r"SOMAXCONN", r"IPPROTO_[A-Z0-9]+", r"IP_[A-Z_]+", r"IPV6_[A-Z_]+", r"TCP_[A-Z_]+",
    r"MSG_[A-Z_]+", r"SHUT_[A-Z]+", r"SCM_[A-Z]+", r"EAI_[A-Z]+", r"AI_[A-Z]+", r"NI_[A-Z]+",
    r"INADDR_[A-Z]+", r"INET6?_ADDRSTRLEN",
    r"POLL[A-Z]+", r"EPOLL[A-Z_]+", r"EFD_[A-Z]+",
    r"CLOCK_[A-Z_]+", r"TIMER_ABSTIME", r"CLOCKS_PER_SEC",
    r"RTLD_[A-Z]+",
    r"PTHREAD_[A-Z_]+", r"SCHED_[A-Z]+",
    r"_SC_[A-Z0-9_]+", r"_PC_[A-Z_]+",
    r"DT_[A-Z]+",
    r"FIO[A-Z]+", r"TIOC[A-Z]+", r"TCGETS", r"TCSETS",
    r"LC_[A-Z_]+",
    r"RLIMIT_[A-Z]+", r"RLIM_INFINITY", r"PRIO_[A-Z]+", r"RUSAGE_[A-Z]+",
    r"PR_[A-Z_0-9]+",
    r"WNOHANG", r"WUNTRACED",
    r"BUFSIZ", r"EOF", r"FILENAME_MAX", r"L_tmpnam", r"TMP_MAX", r"_IO[FLN]BF",
    r"PATH_MAX", r"NAME_MAX", r"IOV_MAX", r"PIPE_BUF", r"HOST_NAME_MAX",
    r"IFNAMSIZ", r"IF_NAMESIZE",
    r"ANDROID_LOG_[A-Z]+",
    r"UTIME_(NOW|OMIT)", r"LOCK_(SH|EX|NB|UN)", r"FALLOC_FL_[A-Z_]+", r"ST_(RDONLY|NOSUID|NODEV|NOEXEC|SYNCHRONOUS|MANDLOCK|NOATIME|NODIRATIME|RELATIME)",
    r"FSETLOCKING_[A-Z]+", r"WEOF", r"RENAME_[A-Z]+", r"STATX_[A-Z_]+", r"SYNC_FILE_RANGE_[A-Z_]+",
    r"TCSA[A-Z]+", r"NCCS", r"V(INTR|QUIT|ERASE|KILL|EOF|TIME|MIN|START|STOP|SUSP|EOL|EOL2|WERASE|LNEXT|REPRINT|DISCARD)",
    r"(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXANY|IXOFF|IMAXBEL|IUTF8)",
    r"(OPOST|ONLCR|OCRNL|ONOCR|ONLRET)", r"(CSIZE|CS5|CS6|CS7|CS8|CSTOPB|CREAD|PARENB|PARODD|HUPCL|CLOCAL)",
    r"(ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHONL|NOFLSH|TOSTOP|IEXTEN|ECHOCTL|ECHOKE)",
    r"SCHED_RESET_ON_FORK",
]
FAMILY_RE = re.compile("^(?:" + "|".join(FAMILIES) + ")$")
EXCLUDE = {"SIG_DFL", "SIG_IGN", "SIG_ERR", "SIG_HOLD", "MAP_FAILED", "RTLD_DEFAULT",
           "RTLD_NEXT", "LC_GLOBAL_LOCALE", "EOF_ELIXIR"}


def find_ndk(explicit):
    candidates = [explicit, os.environ.get("ELIXIR_NDK"), os.environ.get("ANDROID_NDK_HOME"),
                  os.environ.get("ANDROID_NDK_ROOT")]
    sdk_roots = [os.environ.get("ANDROID_HOME"), os.environ.get("ANDROID_SDK_ROOT"),
                 os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk")]
    packages = pathlib.Path(os.environ.get("LOCALAPPDATA", "")) / "Packages"
    if packages.is_dir():
        for package in packages.iterdir():
            sdk_roots.append(str(package / "LocalCache" / "Local" / "Android" / "Sdk"))
    for root in sdk_roots:
        if root and (pathlib.Path(root) / "ndk").is_dir():
            versions = sorted((pathlib.Path(root) / "ndk").iterdir(), reverse=True)
            candidates.extend(str(v) for v in versions)
    for candidate in candidates:
        if candidate and (pathlib.Path(candidate) / "toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe").exists():
            return pathlib.Path(candidate)
    sys.exit("Android NDK not found; pass --ndk")


def elf_symbols(data):
    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise ValueError("not ELF64")
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    sections = []
    for i in range(shnum):
        off = shoff + i * shentsize
        name, stype, flags, addr, offset, size, link, info, align, entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, off)
        sections.append((name, stype, offset, size, link, entsize))
    symbols = {}
    for name, stype, offset, size, link, entsize in sections:
        if stype != 2:
            continue
        strtab = sections[link]
        for j in range(size // entsize):
            st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(
                "<IBBHQQ", data, offset + j * entsize)
            end = data.index(b"\0", strtab[2] + st_name)
            symbols[data[strtab[2] + st_name:end].decode()] = (st_shndx, st_value, st_size)
    return symbols, sections


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ndk")
    parser.add_argument("--api", default="31")
    parser.add_argument("--out", default=str(pathlib.Path(__file__).resolve().parent.parent
                                             / "src" / "bionic" / "guest_constants.h"))
    args = parser.parse_args()
    ndk = find_ndk(args.ndk)
    clang = ndk / "toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe"
    target = f"--target=x86_64-linux-android{args.api}"
    work = pathlib.Path(tempfile.mkdtemp(prefix="elixir_consts_"))
    header_src = "".join(f"#include <{h}>\n" for h in HEADERS)
    (work / "headers.c").write_text("#define _GNU_SOURCE 1\n" + header_src)
    macros = subprocess.run([str(clang), target, "-dM", "-E", str(work / "headers.c")],
                            check=True, capture_output=True, text=True).stdout
    names = []
    for line in macros.splitlines():
        m = re.match(r"#define ([A-Za-z_][A-Za-z0-9_]*) (.+)$", line)
        if not m:
            continue
        name = m.group(1)
        if name in EXCLUDE or not FAMILY_RE.match(name):
            continue
        names.append(name)
    names = sorted(set(names))

    rejected = set()
    for _ in range(40):
        live = [n for n in names if n not in rejected]
        body = "#define _GNU_SOURCE 1\n" + header_src + "".join(
            f"__attribute__((used)) const long long elixir_c_{n} = (long long)({n});\n" for n in live)
        (work / "values.c").write_text(body)
        result = subprocess.run([str(clang), target, "-c", "-O0", "-w", "-o", str(work / "values.o"),
                                 str(work / "values.c")], capture_output=True, text=True)
        if result.returncode == 0:
            break
        bad = set(re.findall(r"elixir_c_([A-Za-z0-9_]+)", result.stderr))
        for line_no in re.findall(r"values\.c:(\d+):", result.stderr):
            text = body.splitlines()[int(line_no) - 1]
            m = re.search(r"elixir_c_([A-Za-z0-9_]+)", text)
            if m:
                bad.add(m.group(1))
        if not bad:
            sys.exit(result.stderr)
        rejected |= bad
    else:
        sys.exit("could not converge on a compilable constant set")

    data = (work / "values.o").read_bytes()
    symbols, sections = elf_symbols(data)
    values = {}
    for n in live:
        shndx, value, size = symbols[f"elixir_c_{n}"]
        section = sections[shndx]
        values[n] = struct.unpack_from("<q", data, section[2] + value)[0]

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "// Copyright 2026 Elixir Project Authors",
        "// Licensed under the Apache License, Version 2.0",
        "//",
        "// GENERATED by tools/gen_bionic_constants.py from the Android NDK's",
        f"// x86_64 bionic headers (API {args.api}). Do not edit by hand.",
        "#ifndef ELIXIR_BIONIC_GUEST_CONSTANTS_H_",
        "#define ELIXIR_BIONIC_GUEST_CONSTANTS_H_",
        "",
        "#include <cstdint>",
        "",
        "namespace elixir::bionic::guest {",
        "",
    ]
    for n in sorted(values):
        v = values[n]
        ctype = "std::int64_t" if not (-2**31 <= v < 2**31) else "int"
        lines.append(f"inline constexpr {ctype} k{n} = {v};")
    lines += ["", "}", "", "#endif", ""]
    out.write_text("\n".join(lines), newline="\n")
    print(f"wrote {len(values)} constants to {out} ({len(rejected)} non-integer macros skipped)")


if __name__ == "__main__":
    main()
