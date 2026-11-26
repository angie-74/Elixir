// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The message tables and the strerror_r/__gnu_strerror_r/strsignal/sig2str/
// str2sig semantics are adapted from AOSP bionic
// (references/platform_bionic/libc/private/bionic_errdefs.h,
// libc/private/bionic_sigdefs.h, libc/bionic/strerror.cpp,
// libc/bionic/strsignal.cpp):
//
//   Copyright (C) 2008-2013 The Android Open Source Project
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
#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic {
namespace {

using namespace guest;
using text_internal::g_size;

struct ErrorDef {
  int number;
  const char* message;
  const char* name;
};

constexpr ErrorDef kErrorDefs[] = {
    {0, "Success", "0"},
    {kEPERM, "Operation not permitted", "EPERM"},
    {kENOENT, "No such file or directory", "ENOENT"},
    {kESRCH, "No such process", "ESRCH"},
    {kEINTR, "Interrupted system call", "EINTR"},
    {kEIO, "I/O error", "EIO"},
    {kENXIO, "No such device or address", "ENXIO"},
    {kE2BIG, "Argument list too long", "E2BIG"},
    {kENOEXEC, "Exec format error", "ENOEXEC"},
    {kEBADF, "Bad file descriptor", "EBADF"},
    {kECHILD, "No child processes", "ECHILD"},
    {kEAGAIN, "Try again", "EAGAIN"},
    {kENOMEM, "Out of memory", "ENOMEM"},
    {kEACCES, "Permission denied", "EACCES"},
    {kEFAULT, "Bad address", "EFAULT"},
    {kENOTBLK, "Block device required", "ENOTBLK"},
    {kEBUSY, "Device or resource busy", "EBUSY"},
    {kEEXIST, "File exists", "EEXIST"},
    {kEXDEV, "Cross-device link", "EXDEV"},
    {kENODEV, "No such device", "ENODEV"},
    {kENOTDIR, "Not a directory", "ENOTDIR"},
    {kEISDIR, "Is a directory", "EISDIR"},
    {kEINVAL, "Invalid argument", "EINVAL"},
    {kENFILE, "File table overflow", "ENFILE"},
    {kEMFILE, "Too many open files", "EMFILE"},
    {kENOTTY, "Inappropriate ioctl for device", "ENOTTY"},
    {kETXTBSY, "Text file busy", "ETXTBSY"},
    {kEFBIG, "File too large", "EFBIG"},
    {kENOSPC, "No space left on device", "ENOSPC"},
    {kESPIPE, "Illegal seek", "ESPIPE"},
    {kEROFS, "Read-only file system", "EROFS"},
    {kEMLINK, "Too many links", "EMLINK"},
    {kEPIPE, "Broken pipe", "EPIPE"},
    {kEDOM, "Math argument out of domain of func", "EDOM"},
    {kERANGE, "Math result not representable", "ERANGE"},
    {kEDEADLK, "Resource deadlock would occur", "EDEADLK"},
    {kENAMETOOLONG, "File name too long", "ENAMETOOLONG"},
    {kENOLCK, "No record locks available", "ENOLCK"},
    {kENOSYS, "Function not implemented", "ENOSYS"},
    {kENOTEMPTY, "Directory not empty", "ENOTEMPTY"},
    {kELOOP, "Too many symbolic links encountered", "ELOOP"},
    {kENOMSG, "No message of desired type", "ENOMSG"},
    {kEIDRM, "Identifier removed", "EIDRM"},
    {kECHRNG, "Channel number out of range", "ECHRNG"},
    {kEL2NSYNC, "Level 2 not synchronized", "EL2NSYNC"},
    {kEL3HLT, "Level 3 halted", "EL3HLT"},
    {kEL3RST, "Level 3 reset", "EL3RST"},
    {kELNRNG, "Link number out of range", "ELNRNG"},
    {kEUNATCH, "Protocol driver not attached", "EUNATCH"},
    {kENOCSI, "No CSI structure available", "ENOCSI"},
    {kEL2HLT, "Level 2 halted", "EL2HLT"},
    {kEBADE, "Invalid exchange", "EBADE"},
    {kEBADR, "Invalid request descriptor", "EBADR"},
    {kEXFULL, "Exchange full", "EXFULL"},
    {kENOANO, "No anode", "ENOANO"},
    {kEBADRQC, "Invalid request code", "EBADRQC"},
    {kEBADSLT, "Invalid slot", "EBADSLT"},
    {kEBFONT, "Bad font file format", "EBFONT"},
    {kENOSTR, "Device not a stream", "ENOSTR"},
    {kENODATA, "No data available", "ENODATA"},
    {kETIME, "Timer expired", "ETIME"},
    {kENOSR, "Out of streams resources", "ENOSR"},
    {kENONET, "Machine is not on the network", "ENONET"},
    {kENOPKG, "Package not installed", "ENOPKG"},
    {kEREMOTE, "Object is remote", "EREMOTE"},
    {kENOLINK, "Link has been severed", "ENOLINK"},
    {kEADV, "Advertise error", "EADV"},
    {kESRMNT, "Srmount error", "ESRMNT"},
    {kECOMM, "Communication error on send", "ECOMM"},
    {kEPROTO, "Protocol error", "EPROTO"},
    {kEMULTIHOP, "Multihop attempted", "EMULTIHOP"},
    {kEDOTDOT, "RFS specific error", "EDOTDOT"},
    {kEBADMSG, "Not a data message", "EBADMSG"},
    {kEOVERFLOW, "Value too large for defined data type", "EOVERFLOW"},
    {kENOTUNIQ, "Name not unique on network", "ENOTUNIQ"},
    {kEBADFD, "File descriptor in bad state", "EBADFD"},
    {kEREMCHG, "Remote address changed", "EREMCHG"},
    {kELIBACC, "Can not access a needed shared library", "ELIBACC"},
    {kELIBBAD, "Accessing a corrupted shared library", "ELIBBAD"},
    {kELIBSCN, ".lib section in a.out corrupted", "ELIBSCN"},
    {kELIBMAX, "Attempting to link in too many shared libraries", "ELIBMAX"},
    {kELIBEXEC, "Cannot exec a shared library directly", "ELIBEXEC"},
    {kEILSEQ, "Illegal byte sequence", "EILSEQ"},
    {kERESTART, "Interrupted system call should be restarted", "ERESTART"},
    {kESTRPIPE, "Streams pipe error", "ESTRPIPE"},
    {kEUSERS, "Too many users", "EUSERS"},
    {kENOTSOCK, "Socket operation on non-socket", "ENOTSOCK"},
    {kEDESTADDRREQ, "Destination address required", "EDESTADDRREQ"},
    {kEMSGSIZE, "Message too long", "EMSGSIZE"},
    {kEPROTOTYPE, "Protocol wrong type for socket", "EPROTOTYPE"},
    {kENOPROTOOPT, "Protocol not available", "ENOPROTOOPT"},
    {kEPROTONOSUPPORT, "Protocol not supported", "EPROTONOSUPPORT"},
    {kESOCKTNOSUPPORT, "Socket type not supported", "ESOCKTNOSUPPORT"},
    {kEOPNOTSUPP, "Operation not supported on transport endpoint", "EOPNOTSUPP"},
    {kEPFNOSUPPORT, "Protocol family not supported", "EPFNOSUPPORT"},
    {kEAFNOSUPPORT, "Address family not supported by protocol", "EAFNOSUPPORT"},
    {kEADDRINUSE, "Address already in use", "EADDRINUSE"},
    {kEADDRNOTAVAIL, "Cannot assign requested address", "EADDRNOTAVAIL"},
    {kENETDOWN, "Network is down", "ENETDOWN"},
    {kENETUNREACH, "Network is unreachable", "ENETUNREACH"},
    {kENETRESET, "Network dropped connection because of reset", "ENETRESET"},
    {kECONNABORTED, "Software caused connection abort", "ECONNABORTED"},
    {kECONNRESET, "Connection reset by peer", "ECONNRESET"},
    {kENOBUFS, "No buffer space available", "ENOBUFS"},
    {kEISCONN, "Transport endpoint is already connected", "EISCONN"},
    {kENOTCONN, "Transport endpoint is not connected", "ENOTCONN"},
    {kESHUTDOWN, "Cannot send after transport endpoint shutdown", "ESHUTDOWN"},
    {kETOOMANYREFS, "Too many references: cannot splice", "ETOOMANYREFS"},
    {kETIMEDOUT, "Connection timed out", "ETIMEDOUT"},
    {kECONNREFUSED, "Connection refused", "ECONNREFUSED"},
    {kEHOSTDOWN, "Host is down", "EHOSTDOWN"},
    {kEHOSTUNREACH, "No route to host", "EHOSTUNREACH"},
    {kEALREADY, "Operation already in progress", "EALREADY"},
    {kEINPROGRESS, "Operation now in progress", "EINPROGRESS"},
    {kESTALE, "Stale NFS file handle", "ESTALE"},
    {kEUCLEAN, "Structure needs cleaning", "EUCLEAN"},
    {kENOTNAM, "Not a XENIX named type file", "ENOTNAM"},
    {kENAVAIL, "No XENIX semaphores available", "ENAVAIL"},
    {kEISNAM, "Is a named type file", "EISNAM"},
    {kEREMOTEIO, "Remote I/O error", "EREMOTEIO"},
    {kEDQUOT, "Quota exceeded", "EDQUOT"},
    {kENOMEDIUM, "No medium found", "ENOMEDIUM"},
    {kEMEDIUMTYPE, "Wrong medium type", "EMEDIUMTYPE"},
    {kECANCELED, "Operation Canceled", "ECANCELED"},
    {kENOKEY, "Required key not available", "ENOKEY"},
    {kEKEYEXPIRED, "Key has expired", "EKEYEXPIRED"},
    {kEKEYREVOKED, "Key has been revoked", "EKEYREVOKED"},
    {kEKEYREJECTED, "Key was rejected by service", "EKEYREJECTED"},
    {kEOWNERDEAD, "Owner died", "EOWNERDEAD"},
    {kENOTRECOVERABLE, "State not recoverable", "ENOTRECOVERABLE"},
    {kERFKILL, "Operation not possible due to RF-kill", "ERFKILL"},
    {kEHWPOISON, "Memory page has hardware error", "EHWPOISON"},
};

constexpr int kMaxErrno = kEHWPOISON;

struct ErrorTables {
  const char* messages[kMaxErrno + 1] = {};
  const char* names[kMaxErrno + 1] = {};
};

constexpr ErrorTables BuildErrorTables() {
  ErrorTables tables;
  for (const ErrorDef& def : kErrorDefs) {
    tables.messages[def.number] = def.message;
    tables.names[def.number] = def.name;
  }
  return tables;
}

constexpr ErrorTables kErrorTables = BuildErrorTables();

const char* LookupError(int number) noexcept {
  if (number < 0 || number > kMaxErrno) return nullptr;
  return kErrorTables.messages[number];
}

struct SignalDef {
  int number;
  const char* description;
  const char* abbreviation;
};

constexpr SignalDef kSignalDefs[] = {
    {kSIGHUP, "Hangup", "HUP"},
    {kSIGINT, "Interrupt", "INT"},
    {kSIGQUIT, "Quit", "QUIT"},
    {kSIGILL, "Illegal instruction", "ILL"},
    {kSIGTRAP, "Trap", "TRAP"},
    {kSIGABRT, "Aborted", "ABRT"},
    {kSIGFPE, "Floating point exception", "FPE"},
    {kSIGKILL, "Killed", "KILL"},
    {kSIGBUS, "Bus error", "BUS"},
    {kSIGSEGV, "Segmentation fault", "SEGV"},
    {kSIGPIPE, "Broken pipe", "PIPE"},
    {kSIGALRM, "Alarm clock", "ALRM"},
    {kSIGTERM, "Terminated", "TERM"},
    {kSIGUSR1, "User signal 1", "USR1"},
    {kSIGUSR2, "User signal 2", "USR2"},
    {kSIGCHLD, "Child exited", "CHLD"},
    {kSIGPWR, "Power failure", "PWR"},
    {kSIGWINCH, "Window size changed", "WINCH"},
    {kSIGURG, "Urgent I/O condition", "URG"},
    {kSIGIO, "I/O possible", "IO"},
    {kSIGSTOP, "Stopped (signal)", "STOP"},
    {kSIGTSTP, "Stopped", "TSTP"},
    {kSIGCONT, "Continue", "CONT"},
    {kSIGTTIN, "Stopped (tty input)", "TTIN"},
    {kSIGTTOU, "Stopped (tty output)", "TTOU"},
    {kSIGVTALRM, "Virtual timer expired", "VTALRM"},
    {kSIGPROF, "Profiling timer expired", "PROF"},
    {kSIGXCPU, "CPU time limit exceeded", "XCPU"},
    {kSIGXFSZ, "File size limit exceeded", "XFSZ"},
    {kSIGSTKFLT, "Stack fault", "STKFLT"},
    {kSIGSYS, "Bad system call", "SYS"},
};

constexpr int kGuestSigRtMin = 42;
constexpr int kGuestSigRtMax = 64;

struct SignalTables {
  const char* siglist[kNSIG] = {};
  const char* signame[kNSIG] = {};
};

constexpr SignalTables BuildSignalTables() {
  SignalTables tables;
  for (const SignalDef& def : kSignalDefs) {
    tables.siglist[def.number] = def.description;
    tables.signame[def.number] = def.abbreviation;
  }
  return tables;
}

constexpr SignalTables kSignalTables = BuildSignalTables();
const char* const* const g_sys_siglist = kSignalTables.siglist;
const char* const* const g_sys_signame = kSignalTables.signame;

std::size_t FormatInt(int value, char* out) noexcept {
  char digits[16];
  std::size_t count = 0;
  long long v = value;
  const bool negative = v < 0;
  if (negative) v = -v;
  do {
    digits[count++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v != 0);
  std::size_t length = 0;
  if (negative) out[length++] = '-';
  while (count != 0) out[length++] = digits[--count];
  out[length] = '\0';
  return length;
}

std::size_t FormatPrefixed(char* buffer, std::size_t size, const char* prefix, int value) noexcept {
  char scratch[64];
  const std::size_t prefix_length = std::strlen(prefix);
  std::memcpy(scratch, prefix, prefix_length);
  const std::size_t total = prefix_length + FormatInt(value, scratch + prefix_length);
  if (size != 0) {
    const std::size_t copy = total < size - 1 ? total : size - 1;
    std::memcpy(buffer, scratch, copy);
    buffer[copy] = '\0';
  }
  return total;
}

constexpr std::size_t kMessageBufferSize = 255;
thread_local char t_strerror_buffer[kMessageBufferSize];
thread_local char t_strsignal_buffer[kMessageBufferSize];

int StrerrorR(int error_number, char* buffer, std::size_t buffer_size) noexcept {
  std::size_t length;
  if (const char* message = LookupError(error_number)) {
    length = text_internal::StrlcpyImpl(buffer, message, buffer_size);
  } else {
    length = FormatPrefixed(buffer, buffer_size, "Unknown error ", error_number);
  }
  return length >= buffer_size ? kERANGE : 0;
}

const char* Strsignal(int signal_number, char* buffer, std::size_t buffer_size) noexcept {
  if (signal_number >= kSIGHUP && signal_number <= kSIGSYS) {
    return kSignalTables.siglist[signal_number];
  }
  const char* prefix = "Unknown signal ";
  if (signal_number >= kGuestSigRtMin && signal_number <= kGuestSigRtMax) {
    prefix = "Real-time signal ";
    signal_number -= kGuestSigRtMin;
  }
  const std::size_t length = FormatPrefixed(buffer, buffer_size, prefix, signal_number);
  return length >= buffer_size ? nullptr : buffer;
}

}

namespace text {

const char* ErrorMessage(int guest_errno) noexcept {
  if (const char* message = LookupError(guest_errno)) return message;
  StrerrorR(guest_errno, t_strerror_buffer, sizeof(t_strerror_buffer));
  return t_strerror_buffer;
}

const char* SignalMessage(int guest_signal) noexcept {
  return Strsignal(guest_signal, t_strsignal_buffer, sizeof(t_strsignal_buffer));
}

}
}

using elixir::bionic::text_internal::g_size;

ELIXIR_BIONIC_FN(char*, strerror)(int error_number) noexcept {
  return const_cast<char*>(elixir::bionic::text::ErrorMessage(error_number));
}

ELIXIR_BIONIC_FN(int, strerror_r)(int error_number, char* buffer, g_size buffer_size) noexcept {
  return elixir::bionic::StrerrorR(error_number, buffer, buffer_size);
}

ELIXIR_BIONIC_FN(char*, __gnu_strerror_r)(int error_number, char* buffer, g_size buffer_size) noexcept {
  elixir::bionic::StrerrorR(error_number, buffer, buffer_size);
  return buffer;
}

ELIXIR_BIONIC_FN(const char*, strerrorname_np)(int error_number) noexcept {
  if (error_number < 0 || error_number > elixir::bionic::kMaxErrno) return nullptr;
  return elixir::bionic::kErrorTables.names[error_number];
}

ELIXIR_BIONIC_FN(char*, strsignal)(int signal_number) noexcept {
  return const_cast<char*>(elixir::bionic::text::SignalMessage(signal_number));
}

ELIXIR_BIONIC_FN(int, sig2str)(int sig, char* str) noexcept {
  using namespace elixir::bionic;
  if (sig >= guest::kSIGHUP && sig <= guest::kSIGSYS) {
    std::strcpy(str, kSignalTables.signame[sig]);
    return 0;
  }
  if (sig == kGuestSigRtMin) {
    std::strcpy(str, "RTMIN");
    return 0;
  }
  if (sig == kGuestSigRtMax) {
    std::strcpy(str, "RTMAX");
    return 0;
  }
  if (sig > kGuestSigRtMin && sig < kGuestSigRtMax) {
    if (sig - kGuestSigRtMin <= kGuestSigRtMax - sig) {
      std::strcpy(str, "RTMIN+");
      FormatInt(sig - kGuestSigRtMin, str + 6);
    } else {
      std::strcpy(str, "RTMAX-");
      FormatInt(kGuestSigRtMax - sig, str + 6);
    }
    return 0;
  }
  return -1;
}

ELIXIR_BIONIC_FN(int, str2sig)(const char* str, int* sig) noexcept {
  using namespace elixir::bionic;
  for (int i = guest::kSIGHUP; i <= guest::kSIGSYS; ++i) {
    if (std::strcmp(str, kSignalTables.signame[i]) == 0) {
      *sig = i;
      return 0;
    }
  }
  if (std::strcmp(str, "RTMIN") == 0) {
    *sig = kGuestSigRtMin;
    return 0;
  }
  if (std::strcmp(str, "RTMAX") == 0) {
    *sig = kGuestSigRtMax;
    return 0;
  }
  int base = 0;
  if (std::strncmp(str, "RTMIN+", 6) == 0) {
    base = kGuestSigRtMin;
    str += 5;
  } else if (std::strncmp(str, "RTMAX-", 6) == 0) {
    base = kGuestSigRtMax;
    str += 5;
  }
  const char* p = str;
  while (text_internal::IsSpace(static_cast<unsigned char>(*p))) ++p;
  bool negative = false;
  if (*p == '-' || *p == '+') negative = (*p++ == '-');
  if (!text_internal::IsDigit(static_cast<unsigned char>(*p))) return -1;
  long long offset = 0;
  while (text_internal::IsDigit(static_cast<unsigned char>(*p))) {
    offset = offset * 10 + (*p++ - '0');
    if (offset > 100000) return -1;
  }
  if (*p != '\0') return -1;
  if (negative) offset = -offset;
  const long long result = base + offset;
  const bool regular = base == 0 && result >= guest::kSIGHUP && result <= guest::kSIGSYS;
  const bool realtime = result >= kGuestSigRtMin && result <= kGuestSigRtMax;
  if (!regular && !realtime) return -1;
  *sig = static_cast<int>(result);
  return 0;
}

namespace elixir::bionic::text_internal {

void RegisterMessageExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, strerror);
  ELIXIR_TEXT_ALIAS(table, kLibc, strerror_l, strerror);
  ELIXIR_TEXT_EXPORT(table, kLibc, strerror_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, __gnu_strerror_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, strerrorname_np);
  ELIXIR_TEXT_EXPORT(table, kLibc, strsignal);
  ELIXIR_TEXT_EXPORT(table, kLibc, sig2str);
  ELIXIR_TEXT_EXPORT(table, kLibc, str2sig);
  table.Add(kLibc, "sys_siglist", const_cast<char**>(reinterpret_cast<const char* const*>(g_sys_siglist)));
  table.Add(kLibc, "sys_signame", const_cast<char**>(reinterpret_cast<const char* const*>(g_sys_signame)));
}

}
