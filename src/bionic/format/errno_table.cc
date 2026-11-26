// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The name/description table is adapted from AOSP bionic
// libc/private/bionic_errdefs.h, which carries this notice:
//
//   Copyright (C) 2023 The Android Open Source Project
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
#include <windows.h>

#include <cstdio>

#include "bionic/format/format_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text.h"

namespace elixir::bionic::format::internal {
namespace {

struct ErrnoEntry {
  int number;
  const char* name;
  const char* description;
};

#define ELIXIR_ERRDEF(name, text) {guest::k##name, #name, text},

constexpr ErrnoEntry kErrnoTable[] = {
    {0, "0", "Success"},
    ELIXIR_ERRDEF(EPERM, "Operation not permitted")
    ELIXIR_ERRDEF(ENOENT, "No such file or directory")
    ELIXIR_ERRDEF(ESRCH, "No such process")
    ELIXIR_ERRDEF(EINTR, "Interrupted system call")
    ELIXIR_ERRDEF(EIO, "I/O error")
    ELIXIR_ERRDEF(ENXIO, "No such device or address")
    ELIXIR_ERRDEF(E2BIG, "Argument list too long")
    ELIXIR_ERRDEF(ENOEXEC, "Exec format error")
    ELIXIR_ERRDEF(EBADF, "Bad file descriptor")
    ELIXIR_ERRDEF(ECHILD, "No child processes")
    ELIXIR_ERRDEF(EAGAIN, "Try again")
    ELIXIR_ERRDEF(ENOMEM, "Out of memory")
    ELIXIR_ERRDEF(EACCES, "Permission denied")
    ELIXIR_ERRDEF(EFAULT, "Bad address")
    ELIXIR_ERRDEF(ENOTBLK, "Block device required")
    ELIXIR_ERRDEF(EBUSY, "Device or resource busy")
    ELIXIR_ERRDEF(EEXIST, "File exists")
    ELIXIR_ERRDEF(EXDEV, "Cross-device link")
    ELIXIR_ERRDEF(ENODEV, "No such device")
    ELIXIR_ERRDEF(ENOTDIR, "Not a directory")
    ELIXIR_ERRDEF(EISDIR, "Is a directory")
    ELIXIR_ERRDEF(EINVAL, "Invalid argument")
    ELIXIR_ERRDEF(ENFILE, "File table overflow")
    ELIXIR_ERRDEF(EMFILE, "Too many open files")
    ELIXIR_ERRDEF(ENOTTY, "Inappropriate ioctl for device")
    ELIXIR_ERRDEF(ETXTBSY, "Text file busy")
    ELIXIR_ERRDEF(EFBIG, "File too large")
    ELIXIR_ERRDEF(ENOSPC, "No space left on device")
    ELIXIR_ERRDEF(ESPIPE, "Illegal seek")
    ELIXIR_ERRDEF(EROFS, "Read-only file system")
    ELIXIR_ERRDEF(EMLINK, "Too many links")
    ELIXIR_ERRDEF(EPIPE, "Broken pipe")
    ELIXIR_ERRDEF(EDOM, "Math argument out of domain of func")
    ELIXIR_ERRDEF(ERANGE, "Math result not representable")
    ELIXIR_ERRDEF(EDEADLK, "Resource deadlock would occur")
    ELIXIR_ERRDEF(ENAMETOOLONG, "File name too long")
    ELIXIR_ERRDEF(ENOLCK, "No record locks available")
    ELIXIR_ERRDEF(ENOSYS, "Function not implemented")
    ELIXIR_ERRDEF(ENOTEMPTY, "Directory not empty")
    ELIXIR_ERRDEF(ELOOP, "Too many symbolic links encountered")
    ELIXIR_ERRDEF(ENOMSG, "No message of desired type")
    ELIXIR_ERRDEF(EIDRM, "Identifier removed")
    ELIXIR_ERRDEF(ECHRNG, "Channel number out of range")
    ELIXIR_ERRDEF(EL2NSYNC, "Level 2 not synchronized")
    ELIXIR_ERRDEF(EL3HLT, "Level 3 halted")
    ELIXIR_ERRDEF(EL3RST, "Level 3 reset")
    ELIXIR_ERRDEF(ELNRNG, "Link number out of range")
    ELIXIR_ERRDEF(EUNATCH, "Protocol driver not attached")
    ELIXIR_ERRDEF(ENOCSI, "No CSI structure available")
    ELIXIR_ERRDEF(EL2HLT, "Level 2 halted")
    ELIXIR_ERRDEF(EBADE, "Invalid exchange")
    ELIXIR_ERRDEF(EBADR, "Invalid request descriptor")
    ELIXIR_ERRDEF(EXFULL, "Exchange full")
    ELIXIR_ERRDEF(ENOANO, "No anode")
    ELIXIR_ERRDEF(EBADRQC, "Invalid request code")
    ELIXIR_ERRDEF(EBADSLT, "Invalid slot")
    ELIXIR_ERRDEF(EBFONT, "Bad font file format")
    ELIXIR_ERRDEF(ENOSTR, "Device not a stream")
    ELIXIR_ERRDEF(ENODATA, "No data available")
    ELIXIR_ERRDEF(ETIME, "Timer expired")
    ELIXIR_ERRDEF(ENOSR, "Out of streams resources")
    ELIXIR_ERRDEF(ENONET, "Machine is not on the network")
    ELIXIR_ERRDEF(ENOPKG, "Package not installed")
    ELIXIR_ERRDEF(EREMOTE, "Object is remote")
    ELIXIR_ERRDEF(ENOLINK, "Link has been severed")
    ELIXIR_ERRDEF(EADV, "Advertise error")
    ELIXIR_ERRDEF(ESRMNT, "Srmount error")
    ELIXIR_ERRDEF(ECOMM, "Communication error on send")
    ELIXIR_ERRDEF(EPROTO, "Protocol error")
    ELIXIR_ERRDEF(EMULTIHOP, "Multihop attempted")
    ELIXIR_ERRDEF(EDOTDOT, "RFS specific error")
    ELIXIR_ERRDEF(EBADMSG, "Not a data message")
    ELIXIR_ERRDEF(EOVERFLOW, "Value too large for defined data type")
    ELIXIR_ERRDEF(ENOTUNIQ, "Name not unique on network")
    ELIXIR_ERRDEF(EBADFD, "File descriptor in bad state")
    ELIXIR_ERRDEF(EREMCHG, "Remote address changed")
    ELIXIR_ERRDEF(ELIBACC, "Can not access a needed shared library")
    ELIXIR_ERRDEF(ELIBBAD, "Accessing a corrupted shared library")
    ELIXIR_ERRDEF(ELIBSCN, ".lib section in a.out corrupted")
    ELIXIR_ERRDEF(ELIBMAX, "Attempting to link in too many shared libraries")
    ELIXIR_ERRDEF(ELIBEXEC, "Cannot exec a shared library directly")
    ELIXIR_ERRDEF(EILSEQ, "Illegal byte sequence")
    ELIXIR_ERRDEF(ERESTART, "Interrupted system call should be restarted")
    ELIXIR_ERRDEF(ESTRPIPE, "Streams pipe error")
    ELIXIR_ERRDEF(EUSERS, "Too many users")
    ELIXIR_ERRDEF(ENOTSOCK, "Socket operation on non-socket")
    ELIXIR_ERRDEF(EDESTADDRREQ, "Destination address required")
    ELIXIR_ERRDEF(EMSGSIZE, "Message too long")
    ELIXIR_ERRDEF(EPROTOTYPE, "Protocol wrong type for socket")
    ELIXIR_ERRDEF(ENOPROTOOPT, "Protocol not available")
    ELIXIR_ERRDEF(EPROTONOSUPPORT, "Protocol not supported")
    ELIXIR_ERRDEF(ESOCKTNOSUPPORT, "Socket type not supported")
    ELIXIR_ERRDEF(EOPNOTSUPP, "Operation not supported on transport endpoint")
    ELIXIR_ERRDEF(EPFNOSUPPORT, "Protocol family not supported")
    ELIXIR_ERRDEF(EAFNOSUPPORT, "Address family not supported by protocol")
    ELIXIR_ERRDEF(EADDRINUSE, "Address already in use")
    ELIXIR_ERRDEF(EADDRNOTAVAIL, "Cannot assign requested address")
    ELIXIR_ERRDEF(ENETDOWN, "Network is down")
    ELIXIR_ERRDEF(ENETUNREACH, "Network is unreachable")
    ELIXIR_ERRDEF(ENETRESET, "Network dropped connection because of reset")
    ELIXIR_ERRDEF(ECONNABORTED, "Software caused connection abort")
    ELIXIR_ERRDEF(ECONNRESET, "Connection reset by peer")
    ELIXIR_ERRDEF(ENOBUFS, "No buffer space available")
    ELIXIR_ERRDEF(EISCONN, "Transport endpoint is already connected")
    ELIXIR_ERRDEF(ENOTCONN, "Transport endpoint is not connected")
    ELIXIR_ERRDEF(ESHUTDOWN, "Cannot send after transport endpoint shutdown")
    ELIXIR_ERRDEF(ETOOMANYREFS, "Too many references: cannot splice")
    ELIXIR_ERRDEF(ETIMEDOUT, "Connection timed out")
    ELIXIR_ERRDEF(ECONNREFUSED, "Connection refused")
    ELIXIR_ERRDEF(EHOSTDOWN, "Host is down")
    ELIXIR_ERRDEF(EHOSTUNREACH, "No route to host")
    ELIXIR_ERRDEF(EALREADY, "Operation already in progress")
    ELIXIR_ERRDEF(EINPROGRESS, "Operation now in progress")
    ELIXIR_ERRDEF(ESTALE, "Stale NFS file handle")
    ELIXIR_ERRDEF(EUCLEAN, "Structure needs cleaning")
    ELIXIR_ERRDEF(ENOTNAM, "Not a XENIX named type file")
    ELIXIR_ERRDEF(ENAVAIL, "No XENIX semaphores available")
    ELIXIR_ERRDEF(EISNAM, "Is a named type file")
    ELIXIR_ERRDEF(EREMOTEIO, "Remote I/O error")
    ELIXIR_ERRDEF(EDQUOT, "Quota exceeded")
    ELIXIR_ERRDEF(ENOMEDIUM, "No medium found")
    ELIXIR_ERRDEF(EMEDIUMTYPE, "Wrong medium type")
    ELIXIR_ERRDEF(ECANCELED, "Operation Canceled")
    ELIXIR_ERRDEF(ENOKEY, "Required key not available")
    ELIXIR_ERRDEF(EKEYEXPIRED, "Key has expired")
    ELIXIR_ERRDEF(EKEYREVOKED, "Key has been revoked")
    ELIXIR_ERRDEF(EKEYREJECTED, "Key was rejected by service")
    ELIXIR_ERRDEF(EOWNERDEAD, "Owner died")
    ELIXIR_ERRDEF(ENOTRECOVERABLE, "State not recoverable")
    ELIXIR_ERRDEF(ERFKILL, "Operation not possible due to RF-kill")
    ELIXIR_ERRDEF(EHWPOISON, "Memory page has hardware error")
};

#undef ELIXIR_ERRDEF

const ErrnoEntry* FindErrno(int number) noexcept {
  for (const ErrnoEntry& entry : kErrnoTable) {
    if (entry.number == number) return &entry;
  }
  return nullptr;
}

}

const char* ErrnoName(int guest_errno) noexcept {
  const ErrnoEntry* entry = FindErrno(guest_errno);
  return entry != nullptr ? entry->name : nullptr;
}

const char* ErrnoMessage(int guest_errno) noexcept { return text::ErrorMessage(guest_errno); }

[[noreturn]] void AbortProcess() noexcept {
  RaiseFailFastException(nullptr, nullptr, 0);
  TerminateProcess(GetCurrentProcess(), 134);
  for (;;) {
  }
}

[[noreturn]] void FatalError(const char* message) noexcept {
  HostLog(kLogFatal, "libc", message);
  AbortProcess();
}

}

extern "C" const char* elixir_format_fallback_error_message(int guest_errno) {
  using elixir::bionic::format::internal::kErrnoTable;
  for (const auto& entry : kErrnoTable) {
    if (entry.number == guest_errno) return entry.description;
  }
  thread_local char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "Unknown error %d", guest_errno);
  return buffer;
}
#pragma comment(linker, \
                "/alternatename:?ErrorMessage@text@bionic@elixir@@YAPEBDH@Z=elixir_format_fallback_error_message")
