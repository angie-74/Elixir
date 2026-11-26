// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/runtime.h"

#include <windows.h>
#include <winsock2.h>

#include <cerrno>
#include <cstdio>
#include <mutex>

#include "platform/windows/guest_tls.h"

namespace elixir::bionic {
namespace {

thread_local int t_guest_errno = 0;
RuntimeOptions g_options;
bool g_initialized = false;

}

int* GuestErrnoLocation() noexcept { return &t_guest_errno; }

int ErrnoFromWin32(std::uint32_t error) noexcept {
  using namespace guest;
  switch (error) {
    case ERROR_SUCCESS: return 0;
    case ERROR_INVALID_FUNCTION: return kEINVAL;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_NO_MORE_FILES:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_MOD_NOT_FOUND:
    case ERROR_DELETE_PENDING: return kENOENT;
    case ERROR_TOO_MANY_OPEN_FILES: return kEMFILE;
    case ERROR_ACCESS_DENIED:
    case ERROR_NETWORK_ACCESS_DENIED:
    case ERROR_CANNOT_MAKE:
    case ERROR_INVALID_ACCESS: return kEACCES;
    case ERROR_INVALID_HANDLE: return kEBADF;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_COMMITMENT_LIMIT: return kENOMEM;
    case ERROR_CURRENT_DIRECTORY:
    case ERROR_BUSY:
    case ERROR_PIPE_BUSY:
    case ERROR_LOCK_VIOLATION:
    case ERROR_LOCK_FAILED:
    case ERROR_SHARING_VIOLATION: return kEBUSY;
    case ERROR_NOT_SAME_DEVICE: return kEXDEV;
    case ERROR_WRITE_PROTECT: return kEROFS;
    case ERROR_CRC:
    case ERROR_GEN_FAILURE:
    case ERROR_IO_DEVICE: return kEIO;
    case ERROR_NOT_READY: return kEAGAIN;
    case ERROR_HANDLE_EOF: return 0;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL: return kENOSPC;
    case ERROR_NOT_SUPPORTED: return kENOTSUP;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS: return kEEXIST;
    case ERROR_INVALID_PARAMETER:
    case ERROR_NEGATIVE_SEEK:
    case ERROR_INVALID_ADDRESS:
    case ERROR_NOT_A_REPARSE_POINT: return kEINVAL;
    case ERROR_BROKEN_PIPE:
    case ERROR_NO_DATA:
    case ERROR_PIPE_NOT_CONNECTED: return kEPIPE;
    case ERROR_CALL_NOT_IMPLEMENTED:
    case ERROR_PROC_NOT_FOUND: return kENOSYS;
    case ERROR_INSUFFICIENT_BUFFER: return kERANGE;
    case ERROR_SEEK_ON_DEVICE: return kESPIPE;
    case ERROR_DIR_NOT_EMPTY: return kENOTEMPTY;
    case ERROR_FILENAME_EXCED_RANGE: return kENAMETOOLONG;
    case ERROR_BAD_EXE_FORMAT: return kENOEXEC;
    case ERROR_DIRECTORY: return kENOTDIR;
    case ERROR_OPERATION_ABORTED: return kEINTR;
    case ERROR_IO_PENDING: return kEINPROGRESS;
    case ERROR_NOACCESS: return kEFAULT;
    case WAIT_TIMEOUT:
    case ERROR_TIMEOUT: return kETIMEDOUT;
    case ERROR_PRIVILEGE_NOT_HELD: return kEPERM;
    case ERROR_CANT_RESOLVE_FILENAME:
    case ERROR_STOPPED_ON_SYMLINK: return kELOOP;
    case ERROR_NOT_LOCKED: return kENOLCK;
    default: return kEIO;
  }
}

int ErrnoFromWinsock(int error) noexcept {
  using namespace guest;
  switch (error) {
    case 0: return 0;
    case WSAEINTR: return kEINTR;
    case WSAEBADF: return kEBADF;
    case WSAEACCES: return kEACCES;
    case WSAEFAULT: return kEFAULT;
    case WSAEINVAL: return kEINVAL;
    case WSAEMFILE: return kEMFILE;
    case WSAEWOULDBLOCK: return kEAGAIN;
    case WSAEINPROGRESS: return kEINPROGRESS;
    case WSAEALREADY: return kEALREADY;
    case WSAENOTSOCK: return kENOTSOCK;
    case WSAEDESTADDRREQ: return kEDESTADDRREQ;
    case WSAEMSGSIZE: return kEMSGSIZE;
    case WSAEPROTOTYPE: return kEPROTOTYPE;
    case WSAENOPROTOOPT: return kENOPROTOOPT;
    case WSAEPROTONOSUPPORT: return kEPROTONOSUPPORT;
    case WSAESOCKTNOSUPPORT: return kESOCKTNOSUPPORT;
    case WSAEOPNOTSUPP: return kEOPNOTSUPP;
    case WSAEPFNOSUPPORT: return kEPFNOSUPPORT;
    case WSAEAFNOSUPPORT: return kEAFNOSUPPORT;
    case WSAEADDRINUSE: return kEADDRINUSE;
    case WSAEADDRNOTAVAIL: return kEADDRNOTAVAIL;
    case WSAENETDOWN: return kENETDOWN;
    case WSAENETUNREACH: return kENETUNREACH;
    case WSAENETRESET: return kENETRESET;
    case WSAECONNABORTED: return kECONNABORTED;
    case WSAECONNRESET: return kECONNRESET;
    case WSAENOBUFS: return kENOBUFS;
    case WSAEISCONN: return kEISCONN;
    case WSAENOTCONN: return kENOTCONN;
    case WSAESHUTDOWN:
    case WSAEDISCON: return kESHUTDOWN;
    case WSAETOOMANYREFS: return kETOOMANYREFS;
    case WSAETIMEDOUT: return kETIMEDOUT;
    case WSAECONNREFUSED: return kECONNREFUSED;
    case WSAELOOP: return kELOOP;
    case WSAENAMETOOLONG: return kENAMETOOLONG;
    case WSAEHOSTDOWN: return kEHOSTDOWN;
    case WSAEHOSTUNREACH: return kEHOSTUNREACH;
    case WSAENOTEMPTY: return kENOTEMPTY;
    case WSAEUSERS: return kEUSERS;
    case WSAEDQUOT: return kEDQUOT;
    case WSAESTALE: return kESTALE;
    case WSAEREMOTE: return kEREMOTE;
    case WSANOTINITIALISED:
    case WSASYSNOTREADY: return kENETDOWN;
    default: return kEIO;
  }
}

int ErrnoFromCrt(int error) noexcept {
  using namespace guest;
  if (error >= 0 && error <= 34) return error;
  switch (error) {
    case EDEADLK: return kEDEADLK;
    case ENAMETOOLONG: return kENAMETOOLONG;
    case ENOLCK: return kENOLCK;
    case ENOSYS: return kENOSYS;
    case ENOTEMPTY: return kENOTEMPTY;
    case EILSEQ: return kEILSEQ;
    case EADDRINUSE: return kEADDRINUSE;
    case EADDRNOTAVAIL: return kEADDRNOTAVAIL;
    case EAFNOSUPPORT: return kEAFNOSUPPORT;
    case EALREADY: return kEALREADY;
    case EBADMSG: return kEBADMSG;
    case ECANCELED: return kECANCELED;
    case ECONNABORTED: return kECONNABORTED;
    case ECONNREFUSED: return kECONNREFUSED;
    case ECONNRESET: return kECONNRESET;
    case EDESTADDRREQ: return kEDESTADDRREQ;
    case EHOSTUNREACH: return kEHOSTUNREACH;
    case EIDRM: return kEIDRM;
    case EINPROGRESS: return kEINPROGRESS;
    case EISCONN: return kEISCONN;
    case ELOOP: return kELOOP;
    case EMSGSIZE: return kEMSGSIZE;
    case ENETDOWN: return kENETDOWN;
    case ENETRESET: return kENETRESET;
    case ENETUNREACH: return kENETUNREACH;
    case ENOBUFS: return kENOBUFS;
    case ENODATA: return kENODATA;
    case ENOLINK: return kENOLINK;
    case ENOMSG: return kENOMSG;
    case ENOPROTOOPT: return kENOPROTOOPT;
    case ENOSR: return kENOSR;
    case ENOSTR: return kENOSTR;
    case ENOTCONN: return kENOTCONN;
    case ENOTRECOVERABLE: return kENOTRECOVERABLE;
    case ENOTSOCK: return kENOTSOCK;
    case ENOTSUP: return kENOTSUP;
    case EOPNOTSUPP: return kEOPNOTSUPP;
    case EOVERFLOW: return kEOVERFLOW;
    case EOWNERDEAD: return kEOWNERDEAD;
    case EPROTO: return kEPROTO;
    case EPROTONOSUPPORT: return kEPROTONOSUPPORT;
    case EPROTOTYPE: return kEPROTOTYPE;
    case ETIME: return kETIME;
    case ETIMEDOUT: return kETIMEDOUT;
    case ETXTBSY: return kETXTBSY;
    case EWOULDBLOCK: return kEWOULDBLOCK;
    default: return kEIO;
  }
}

void ExportTable::Add(std::string_view soname, std::string_view name, void* address) {
  void*& slot = libraries_[std::string(soname)][std::string(name)];
  if (slot != nullptr && slot != address) conflicts_.push_back(std::string(soname) + ":" + std::string(name));
  slot = address;
}

void* ExportTable::Find(std::string_view soname, std::string_view name) const {
  const auto library = libraries_.find(std::string(soname));
  if (library == libraries_.end()) return nullptr;
  const auto symbol = library->second.find(std::string(name));
  return symbol == library->second.end() ? nullptr : symbol->second;
}

std::size_t ExportTable::size() const {
  std::size_t total = 0;
  for (const auto& [name, symbols] : libraries_) total += symbols.size();
  return total;
}

bool InitializeRuntime(const RuntimeOptions& options) {
  static std::mutex lock;
  std::lock_guard<std::mutex> guard(lock);
  if (g_initialized) return true;
  if (!platform::InitializeGuestTls()) return false;
  g_options = options;
  g_initialized = true;
  return true;
}

const RuntimeOptions& Options() { return g_options; }

void HostLog(int priority, const char* tag, const char* message) noexcept {
  if (g_options.log_sink != nullptr) {
    g_options.log_sink(priority, tag, message);
    return;
  }
  static const char kLevels[] = "??VDIWEFS";
  const char level = priority >= 0 && priority < 9 ? kLevels[priority] : '?';
  std::fprintf(stderr, "%c/%s: %s\n", level, tag != nullptr ? tag : "", message != nullptr ? message : "");
}

}
