// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/guest_symbolizer.h"

#include <windows.h>

#include <mcpelauncher/linker.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "elf/guest_images.h"

namespace elixir::runtime {
namespace {

struct LoaderRequest {
  std::uintptr_t address = 0;
  std::uint64_t sequence = 0;
  int found = 0;
  char module[512] = {};
  std::uintptr_t module_base = 0;
  char symbol[512] = {};
  std::uintptr_t symbol_address = 0;
};

LoaderRequest g_request;
std::atomic<std::uint64_t> g_answered{0};
std::atomic<std::uint64_t> g_next_sequence{0};
std::atomic<bool> g_busy{false};
std::atomic<bool> g_stop{false};
std::atomic<bool> g_running{false};
std::atomic<DWORD> g_worker_thread_id{0};
HANDLE g_worker = nullptr;
HANDLE g_request_event = nullptr;
HANDLE g_done_event = nullptr;
DWORD g_timeout_ms = 500;
SRWLOCK g_requester = SRWLOCK_INIT;
SRWLOCK g_lifecycle = SRWLOCK_INIT;

std::atomic<unsigned long> g_loader_answers{0};
std::atomic<unsigned long> g_loader_misses{0};
std::atomic<unsigned long> g_loader_timeouts{0};
std::atomic<unsigned long> g_loader_skipped{0};
std::atomic<unsigned long> g_image_answers{0};

void CopyText(char* destination, std::size_t capacity, const char* source) {
  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }
  std::size_t length = std::strlen(source);
  if (length >= capacity) length = capacity - 1;
  std::memcpy(destination, source, length);
  destination[length] = '\0';
}

const char* BaseName(const char* path) {
  const char* name = path;
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') name = cursor + 1;
  }
  return name;
}

DWORD WINAPI LoaderWorker(void*) {
  g_worker_thread_id.store(GetCurrentThreadId());
  for (;;) {
    WaitForSingleObject(g_request_event, INFINITE);
    if (g_stop.load(std::memory_order_acquire)) break;
    const std::uint64_t sequence = g_request.sequence;
    Dl_info info{};
    const int found = linker::dladdr(reinterpret_cast<const void*>(g_request.address), &info);
    g_request.found = found != 0 && info.dli_fname != nullptr;
    if (g_request.found) {
      CopyText(g_request.module, sizeof(g_request.module), info.dli_fname);
      g_request.module_base = reinterpret_cast<std::uintptr_t>(info.dli_fbase);
      CopyText(g_request.symbol, sizeof(g_request.symbol), info.dli_sname);
      g_request.symbol_address = reinterpret_cast<std::uintptr_t>(info.dli_saddr);
    }
    g_answered.store(sequence, std::memory_order_release);
    g_busy.store(false, std::memory_order_release);
    SetEvent(g_done_event);
  }
  return 0;
}

int AskLoader(std::uintptr_t address, platform::SymbolInfo* info) {
  if (!g_running.load(std::memory_order_acquire) || GetCurrentThreadId() == g_worker_thread_id.load()) {
    ++g_loader_skipped;
    return -1;
  }
  if (!TryAcquireSRWLockExclusive(&g_requester)) {
    ++g_loader_skipped;
    return -1;
  }
  if (!g_running.load(std::memory_order_acquire)) {
    ReleaseSRWLockExclusive(&g_requester);
    ++g_loader_skipped;
    return -1;
  }
  bool expected = false;
  if (!g_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    ReleaseSRWLockExclusive(&g_requester);
    ++g_loader_skipped;
    return -1;
  }
  const std::uint64_t sequence = g_next_sequence.fetch_add(1) + 1;
  g_request.address = address;
  g_request.sequence = sequence;
  ResetEvent(g_done_event);
  SetEvent(g_request_event);
  const ULONGLONG deadline = GetTickCount64() + g_timeout_ms;
  int result = -1;
  for (;;) {
    const ULONGLONG now = GetTickCount64();
    const DWORD remaining = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
    const DWORD wait = WaitForSingleObject(g_done_event, remaining);
    if (g_answered.load(std::memory_order_acquire) == sequence) {
      if (g_request.found) {
        info->module = BaseName(g_request.module);
        info->module_offset = address - g_request.module_base;
        info->symbol = g_request.symbol;
        info->symbol_offset = g_request.symbol[0] != '\0' ? address - g_request.symbol_address : 0;
        ++g_loader_answers;
        result = 1;
      } else {
        ++g_loader_misses;
        result = 0;
      }
      break;
    }
    if (wait != WAIT_OBJECT_0) {
      ++g_loader_timeouts;
      break;
    }
  }
  ReleaseSRWLockExclusive(&g_requester);
  return result;
}

bool IsReadable(std::uintptr_t address, std::size_t size) {
  if (address == 0 || size == 0) return false;
  std::uintptr_t cursor = address;
  const std::uintptr_t end = address + size;
  if (end < address) return false;
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) == 0) return false;
    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        region.Protect == 0) {
      return false;
    }
    cursor = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
  }
  return true;
}

template <typename T>
bool Load(std::uintptr_t address, T* out) {
  if (!IsReadable(address, sizeof(T))) return false;
  std::memcpy(out, reinterpret_cast<const void*>(address), sizeof(T));
  return true;
}

struct Elf64Sym {
  std::uint32_t st_name;
  std::uint8_t st_info;
  std::uint8_t st_other;
  std::uint16_t st_shndx;
  std::uint64_t st_value;
  std::uint64_t st_size;
};

std::uint64_t SymbolCount(std::uintptr_t gnu_hash, std::uintptr_t sysv_hash) {
  if (gnu_hash != 0) {
    std::uint32_t header[4];
    if (!Load(gnu_hash, &header)) return 0;
    const std::uint32_t buckets = header[0], symoffset = header[1], bloom_words = header[2];
    const std::uintptr_t bucket_table = gnu_hash + 16 + static_cast<std::uintptr_t>(bloom_words) * 8;
    if (!IsReadable(bucket_table, static_cast<std::size_t>(buckets) * 4)) return 0;
    std::uint32_t last = 0;
    for (std::uint32_t index = 0; index < buckets; ++index) {
      const std::uint32_t value = reinterpret_cast<const std::uint32_t*>(bucket_table)[index];
      if (value > last) last = value;
    }
    if (last < symoffset) return symoffset;
    const std::uintptr_t chains = bucket_table + static_cast<std::uintptr_t>(buckets) * 4;
    for (std::uint64_t symbol = last; symbol - last < (1U << 24); ++symbol) {
      std::uint32_t hash = 0;
      if (!Load(chains + (symbol - symoffset) * 4, &hash)) return 0;
      if ((hash & 1U) != 0) return symbol + 1;
    }
    return 0;
  }
  if (sysv_hash != 0) {
    std::uint32_t header[2];
    return Load(sysv_hash, &header) ? header[1] : 0;
  }
  return 0;
}

bool ReadString(std::uintptr_t strtab, std::uint64_t strsz, std::uint64_t offset, std::string* out) {
  if (offset >= strsz) return false;
  const std::uintptr_t begin = strtab + offset;
  std::string text;
  for (std::uintptr_t cursor = begin; cursor < strtab + strsz && text.size() < 1024; ++cursor) {
    char c = 0;
    if (!Load(cursor, &c)) return false;
    if (c == '\0') {
      *out = std::move(text);
      return true;
    }
    text.push_back(c);
  }
  return false;
}

}

namespace {

struct ImageLayout {
  std::uintptr_t base = 0;
  std::uintptr_t bias = 0;
  std::uint64_t dynamic_vaddr = 0;
  std::uint64_t dynamic_size = 0;
};

bool ParseImageAt(std::uintptr_t candidate, std::uintptr_t address, ImageLayout* layout) {
  unsigned char header[64];
  if (!IsReadable(candidate, sizeof(header))) return false;
  std::memcpy(header, reinterpret_cast<const void*>(candidate), sizeof(header));
  if (std::memcmp(header, "\x7f" "ELF", 4) != 0 || header[4] != 2 || header[5] != 1) return false;
  std::uint64_t phoff = 0;
  std::uint16_t phentsize = 0, phnum = 0;
  std::memcpy(&phoff, header + 32, 8);
  std::memcpy(&phentsize, header + 54, 2);
  std::memcpy(&phnum, header + 56, 2);
  if (phentsize != 56 || phnum == 0 || phnum > 256 || phoff > (1U << 20) ||
      !IsReadable(candidate + phoff, std::size_t{56} * phnum)) {
    return false;
  }
  std::uint64_t min_vaddr = ~0ULL;
  for (std::uint16_t index = 0; index < phnum; ++index) {
    const std::uintptr_t entry = candidate + phoff + std::uintptr_t{56} * index;
    std::uint32_t type = 0;
    std::uint64_t vaddr = 0;
    std::memcpy(&type, reinterpret_cast<const void*>(entry), 4);
    std::memcpy(&vaddr, reinterpret_cast<const void*>(entry + 16), 8);
    if (type == 1 && vaddr < min_vaddr) min_vaddr = vaddr;
  }
  if (min_vaddr == ~0ULL) return false;
  const std::uintptr_t bias = candidate - static_cast<std::uintptr_t>(min_vaddr & ~0xfffULL);
  bool contains = false;
  for (std::uint16_t index = 0; index < phnum; ++index) {
    const std::uintptr_t entry = candidate + phoff + std::uintptr_t{56} * index;
    std::uint32_t type = 0, flags = 0;
    std::uint64_t vaddr = 0, memsz = 0;
    std::memcpy(&type, reinterpret_cast<const void*>(entry), 4);
    std::memcpy(&flags, reinterpret_cast<const void*>(entry + 4), 4);
    std::memcpy(&vaddr, reinterpret_cast<const void*>(entry + 16), 8);
    std::memcpy(&memsz, reinterpret_cast<const void*>(entry + 40), 8);
    if (type == 1 && (flags & 1U) != 0 && address >= bias + vaddr && address - (bias + vaddr) < memsz) {
      contains = true;
    }
    if (type == 2) {
      layout->dynamic_vaddr = vaddr;
      layout->dynamic_size = memsz;
    }
  }
  if (!contains || layout->dynamic_size == 0) return false;
  layout->base = candidate;
  layout->bias = bias;
  return true;
}

bool FindImage(std::uintptr_t address, ImageLayout* layout) {
  MEMORY_BASIC_INFORMATION region{};
  if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) == 0) return false;
  const auto allocation = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
  std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
  for (int step = 0; step < 256; ++step) {
    if (region.State == MEM_COMMIT && ParseImageAt(cursor, address, layout)) return true;
    if (cursor <= allocation || cursor == 0) return false;
    if (VirtualQuery(reinterpret_cast<const void*>(cursor - 1), &region, sizeof(region)) == 0) return false;
    if (reinterpret_cast<std::uintptr_t>(region.AllocationBase) != allocation) return false;
    cursor = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
  }
  return false;
}

}

bool SymbolizeFromLoadedImage(std::uintptr_t address, platform::SymbolInfo* info) {
  ImageLayout layout;
  if (!FindImage(address, &layout)) return false;
  const std::uintptr_t base = layout.base;
  const std::uintptr_t bias = layout.bias;
  const std::uint64_t dynamic_vaddr = layout.dynamic_vaddr;
  const std::uint64_t dynamic_size = layout.dynamic_size;
  std::uintptr_t strtab = 0, symtab = 0, gnu_hash = 0, sysv_hash = 0;
  std::uint64_t strsz = 0, soname = ~0ULL;
  for (std::uint64_t offset = 0; offset + 16 <= dynamic_size && offset < 16 * 4096; offset += 16) {
    std::int64_t tag = 0;
    std::uint64_t value = 0;
    if (!Load(bias + dynamic_vaddr + offset, &tag) || !Load(bias + dynamic_vaddr + offset + 8, &value)) {
      return false;
    }
    if (tag == 0) break;
    switch (tag) {
      case 5: strtab = bias + value; break;
      case 6: symtab = bias + value; break;
      case 10: strsz = value; break;
      case 14: soname = value; break;
      case 4: sysv_hash = bias + value; break;
      case 0x6ffffef5: gnu_hash = bias + value; break;
      default: break;
    }
  }
  if (strtab == 0 || symtab == 0) return false;
  std::string module;
  if (soname == ~0ULL || !ReadString(strtab, strsz, soname, &module)) {
    char name[48];
    std::snprintf(name, sizeof(name), "image@0x%llx", static_cast<unsigned long long>(base));
    module = name;
  }
  const std::uint64_t relative = address - bias;
  const std::uint64_t count = SymbolCount(gnu_hash, sysv_hash);
  bool have_best = false;
  Elf64Sym best_copy{};
  if (count > 1 && count < (1U << 24) && IsReadable(symtab, static_cast<std::size_t>(count) * sizeof(Elf64Sym))) {
    const auto* symbols = reinterpret_cast<const Elf64Sym*>(symtab);
    for (std::uint64_t index = 1; index < count; ++index) {
      const Elf64Sym& symbol = symbols[index];
      const unsigned type = symbol.st_info & 15U;
      if (symbol.st_shndx == 0 || symbol.st_value == 0 || symbol.st_value > relative) continue;
      if (type != 2 && type != 10) continue;
      if (symbol.st_size != 0 && relative < symbol.st_value + symbol.st_size) {
        best_copy = symbol;
        have_best = true;
        break;
      }
      if (!have_best || symbol.st_value > best_copy.st_value) {
        best_copy = symbol;
        have_best = true;
      }
    }
  }
  info->module = module;
  info->module_offset = address - base;
  info->symbol.clear();
  info->symbol_offset = 0;
  if (have_best) {
    std::string name;
    if (ReadString(strtab, strsz, best_copy.st_name, &name)) {
      info->symbol = name;
      info->symbol_offset = relative - best_copy.st_value;
    }
  }
  ++g_image_answers;
  return true;
}

bool StartGuestSymbolizer(const GuestSymbolizerOptions& options) {
  AcquireSRWLockExclusive(&g_lifecycle);
  bool ok = g_running.load();
  if (!ok && g_worker != nullptr && WaitForSingleObject(g_worker, 0) == WAIT_OBJECT_0) {
    CloseHandle(g_worker);
    CloseHandle(g_request_event);
    CloseHandle(g_done_event);
    g_worker = nullptr;
    g_request_event = g_done_event = nullptr;
    g_worker_thread_id.store(0);
  }
  if (!ok && g_worker == nullptr) {
    g_timeout_ms = options.loader_timeout_ms;
    g_stop.store(false);
    g_busy.store(false);
    g_request_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_done_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_worker = g_request_event != nullptr && g_done_event != nullptr
                   ? CreateThread(nullptr, 256 * 1024, &LoaderWorker, nullptr, 0, nullptr)
                   : nullptr;
    ok = g_worker != nullptr;
    if (!ok) {
      if (g_request_event != nullptr) CloseHandle(g_request_event);
      if (g_done_event != nullptr) CloseHandle(g_done_event);
      g_request_event = g_done_event = nullptr;
    }
    g_running.store(ok);
  }
  ReleaseSRWLockExclusive(&g_lifecycle);
  return ok;
}

bool StopGuestSymbolizer() {
  AcquireSRWLockExclusive(&g_lifecycle);
  bool joined = true;
  if (g_worker != nullptr) {
    g_running.store(false);
    AcquireSRWLockExclusive(&g_requester);
    g_stop.store(true, std::memory_order_release);
    SetEvent(g_request_event);
    joined = WaitForSingleObject(g_worker, 2000) == WAIT_OBJECT_0;
    if (joined) {
      CloseHandle(g_worker);
      CloseHandle(g_request_event);
      CloseHandle(g_done_event);
      g_worker = nullptr;
      g_request_event = g_done_event = nullptr;
      g_worker_thread_id.store(0);
    }
    ReleaseSRWLockExclusive(&g_requester);
  }
  ReleaseSRWLockExclusive(&g_lifecycle);
  return joined;
}

bool GuestSymbolizerRunning() { return g_running.load(); }

bool SymbolizeGuestAddress(std::uintptr_t address, platform::SymbolInfo* info) {
  if (info == nullptr || !elf::IsGuestCode(address)) return false;
  const int loader = AskLoader(address, info);
  if (loader == 1) return true;
  return SymbolizeFromLoadedImage(address, info);
}

GuestSymbolizerStats GuestSymbolizerStatistics() {
  GuestSymbolizerStats stats;
  stats.loader_answers = g_loader_answers.load();
  stats.loader_misses = g_loader_misses.load();
  stats.loader_timeouts = g_loader_timeouts.load();
  stats.loader_skipped = g_loader_skipped.load();
  stats.image_answers = g_image_answers.load();
  return stats;
}

}
