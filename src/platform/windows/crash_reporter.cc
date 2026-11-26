// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "platform/windows/crash_reporter.h"
#include "platform/windows/guest_unwind_barrier.h"

#include <windows.h>
#include <dbghelp.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>

namespace elixir::platform {
namespace {

std::mutex g_lock;
CrashReporterOptions g_options;
bool g_installed = false;
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;

const char* ExceptionName(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
    case 0xC0000602: return "STATUS_FAIL_FAST_EXCEPTION";
    case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN";
    case 0xE06D7363: return "C++ exception";
    default: return "unknown exception";
  }
}

void Append(std::string& out, const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (written > 0) out.append(buffer, static_cast<std::size_t>(written < 511 ? written : 511));
}

bool IsExecutable(std::uintptr_t address) {
  MEMORY_BASIC_INFORMATION info{};
  if (address < 0x10000 || VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0) return false;
  if (info.State != MEM_COMMIT) return false;
  const DWORD protect = info.Protect & 0xff;
  return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE ||
         protect == PAGE_EXECUTE_WRITECOPY;
}

std::string Describe(std::uintptr_t address, const Symbolizer& guest) {
  SymbolInfo info;
  if (guest && guest(address, &info)) {
    std::string out = info.module + "+0x";
    char hex[32];
    std::snprintf(hex, sizeof(hex), "%llx", static_cast<unsigned long long>(info.module_offset));
    out += hex;
    if (!info.symbol.empty()) {
      std::snprintf(hex, sizeof(hex), "%llx", static_cast<unsigned long long>(info.symbol_offset));
      out += " (" + info.symbol + "+0x" + hex + ")";
    }
    return out + " [guest]";
  }
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(address), &module)) {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring wide(path, length);
    const std::size_t slash = wide.find_last_of(L"\\/");
    if (slash != std::wstring::npos) wide = wide.substr(slash + 1);
    char narrow[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, narrow, sizeof(narrow), nullptr, nullptr);
    char hex[32];
    std::snprintf(hex, sizeof(hex), "+0x%llx",
                  static_cast<unsigned long long>(address - reinterpret_cast<std::uintptr_t>(module)));
    return std::string(narrow) + hex + " [host]";
  }
  return IsExecutable(address) ? "<anonymous executable memory>" : "<unknown>";
}

std::wstring Timestamp() {
  SYSTEMTIME now;
  GetSystemTime(&now);
  wchar_t buffer[64];
  std::swprintf(buffer, 64, L"%04u%02u%02u-%02u%02u%02u-%03u", now.wYear, now.wMonth, now.wDay, now.wHour,
                now.wMinute, now.wSecond, now.wMilliseconds);
  return buffer;
}

bool EnsureDirectory(const std::wstring& directory) {
  if (directory.empty()) return false;
  std::wstring partial;
  for (std::size_t i = 0; i < directory.size(); ++i) {
    partial.push_back(directory[i]);
    if ((directory[i] == L'\\' || directory[i] == L'/') && partial.size() > 3) CreateDirectoryW(partial.c_str(), nullptr);
  }
  CreateDirectoryW(directory.c_str(), nullptr);
  return GetFileAttributesW(directory.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void WriteReport(EXCEPTION_POINTERS* pointers, const std::string& reason) {
  CrashReporterOptions options;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    options = g_options;
  }
  if (options.directory.empty()) options.directory = DefaultCrashDirectory();
  if (!EnsureDirectory(options.directory)) {
    std::string report = FormatCrashReport(pointers);
    if (!reason.empty()) report = "reason: " + reason + "\n" + report;
    std::fputs(report.c_str(), stderr);
    std::fflush(stderr);
    return;
  }
  const std::wstring base = options.directory + L"\\crash-" + Timestamp();
  std::string report = FormatCrashReport(pointers);
  if (!reason.empty()) {
    const std::size_t first_line = report.find('\n');
    report.insert(first_line == std::string::npos ? report.size() : first_line + 1, "reason: " + reason + "\n");
  }
  HANDLE text = CreateFileW((base + L".txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (text != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    WriteFile(text, report.data(), static_cast<DWORD>(report.size()), &written, nullptr);
    CloseHandle(text);
  }
  if (options.write_minidump) {
    HANDLE dump = CreateFileW((base + L".dmp").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(), pointers, FALSE};
      const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory |
                                                   MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData);
      MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump, type, &exception, nullptr, nullptr);
      CloseHandle(dump);
    }
  }
  std::fputs(report.c_str(), stderr);
  std::fflush(stderr);
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* pointers) {
  WriteReport(pointers, {});
  if (g_previous_filter != nullptr) return g_previous_filter(pointers);
  return EXCEPTION_EXECUTE_HANDLER;
}

constexpr DWORD kElixirSoftwareFailure = 0xE0E11A01;

[[noreturn]] void ReportAndExit(std::string reason) {
  static std::atomic<bool> reporting{false};
  if (!reporting.exchange(true)) {
    CONTEXT context{};
    RtlCaptureContext(&context);
    EXCEPTION_RECORD record{};
    record.ExceptionCode = kElixirSoftwareFailure;
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
    EXCEPTION_POINTERS pointers{&record, &context};
    WriteReport(&pointers, reason);
  }
  TerminateProcess(GetCurrentProcess(), 3);
  for (;;) Sleep(INFINITE);
}

void OnTerminate() {
  std::string reason = "std::terminate";
  if (std::exception_ptr current = std::current_exception()) {
    try {
      std::rethrow_exception(current);
    } catch (const std::exception& error) {
      reason += " with uncaught exception: ";
      reason += error.what();
    } catch (...) {
      reason += " with an uncaught non-std exception";
    }
  } else {
    reason += " (no active exception: noexcept violation or explicit call)";
  }
  ReportAndExit(std::move(reason));
}

void OnAbortSignal(int) { ReportAndExit("abort()"); }

void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
  ReportAndExit("CRT invalid parameter");
}

}

void ReportFatalException(const void* exception_pointers, const char* reason) noexcept {
  try {
    WriteReport(static_cast<EXCEPTION_POINTERS*>(const_cast<void*>(exception_pointers)), reason != nullptr ? reason : "");
  } catch (...) {
  }
}

std::wstring DefaultCrashDirectory() {
  wchar_t buffer[MAX_PATH];
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return L".\\crashes";
  return std::wstring(buffer, length) + L"\\Elixir\\crashes";
}

std::string FormatCrashReport(const void* exception_pointers) {
  const auto* pointers = static_cast<const EXCEPTION_POINTERS*>(exception_pointers);
  const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
  const CONTEXT* context = pointers->ContextRecord;
  Symbolizer symbolizer;
  std::string version;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    symbolizer = g_options.guest_symbolizer;
    version = g_options.product_version;
  }
  std::string out;
  Append(out, "Elixir crash report\n");
  if (!version.empty()) Append(out, "version: %s\n", version.c_str());
  Append(out, "process: %lu thread: %lu\n", GetCurrentProcessId(), GetCurrentThreadId());
  Append(out, "exception: 0x%08lx %s at %p\n", record->ExceptionCode, ExceptionName(record->ExceptionCode),
         record->ExceptionAddress);
  if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
    const ULONG_PTR kind = record->ExceptionInformation[0];
    Append(out, "fault: %s of address 0x%llx\n", kind == 0 ? "read" : kind == 1 ? "write" : "execute",
           static_cast<unsigned long long>(record->ExceptionInformation[1]));
  }
  Append(out, "rip: %s\n", Describe(static_cast<std::uintptr_t>(context->Rip), symbolizer).c_str());
  Append(out, "\nregisters:\n");
  Append(out, "  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n", context->Rax, context->Rbx, context->Rcx,
         context->Rdx);
  Append(out, "  rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx\n", context->Rsi, context->Rdi, context->Rbp,
         context->Rsp);
  Append(out, "  r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n", context->R8, context->R9, context->R10,
         context->R11);
  Append(out, "  r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n", context->R12, context->R13, context->R14,
         context->R15);
  Append(out, "  rip=%016llx eflags=%08lx mxcsr=%08lx\n", context->Rip, context->EFlags, context->MxCsr);

  Append(out, "\nhost call chain (unwound):\n");
  {
    CONTEXT walk = *context;
    for (int depth = 0; depth < 64; ++depth) {
      const auto rip = static_cast<std::uintptr_t>(walk.Rip);
      if (rip == 0) break;
      Append(out, "  #%02d %016llx  %s\n", depth, static_cast<unsigned long long>(rip), Describe(rip, symbolizer).c_str());
      if (IsBehindGuestUnwindBarrier(rip)) {
        Append(out, "  (guest frame: continue with the stack scan)\n");
        break;
      }
      DWORD64 image_base = 0;
      RUNTIME_FUNCTION* function = RtlLookupFunctionEntry(walk.Rip, &image_base, nullptr);
      if (function == nullptr) {
        if (depth != 0) {
          Append(out, "  (frame without unwind data: guest code or JIT; see the stack scan)\n");
          break;
        }
        walk.Rip = *reinterpret_cast<const DWORD64*>(walk.Rsp);
        walk.Rsp += 8;
        continue;
      }
      void* handler_data = nullptr;
      DWORD64 establisher = 0;
      RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, walk.Rip, function, &walk, &handler_data, &establisher, nullptr);
    }
  }

  Append(out, "\nprobable return addresses (stack scan from rsp):\n");
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  std::uintptr_t cursor = static_cast<std::uintptr_t>(context->Rsp);
  std::uintptr_t limit = cursor + 64 * 1024;
  if (cursor < low || cursor >= high) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) != 0) {
      high = reinterpret_cast<ULONG_PTR>(info.BaseAddress) + info.RegionSize;
    }
  }
  if (limit > high) limit = high;
  int found = 0;
  for (; cursor + 8 <= limit && found < 48; cursor += 8) {
    const std::uintptr_t value = *reinterpret_cast<const std::uintptr_t*>(cursor);
    if (!IsExecutable(value)) continue;
    Append(out, "  [rsp+0x%05llx] %016llx  %s\n", static_cast<unsigned long long>(cursor - context->Rsp),
           static_cast<unsigned long long>(value), Describe(value, symbolizer).c_str());
    ++found;
  }
  if (found == 0) Append(out, "  (none found)\n");
  return out;
}

bool InstallCrashReporter(const CrashReporterOptions& options) {
  std::lock_guard<std::mutex> guard(g_lock);
  g_options = options;
  if (g_options.directory.empty()) g_options.directory = DefaultCrashDirectory();
  if (!g_installed) {
    g_previous_filter = SetUnhandledExceptionFilter(UnhandledFilter);
    std::set_terminate(&OnTerminate);
    std::signal(SIGABRT, &OnAbortSignal);
    _set_invalid_parameter_handler(&OnInvalidParameter);
    g_installed = true;
  }
  return true;
}

}
