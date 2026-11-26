// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/guest_ext.h"

#include <mcpelauncher/linker.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/dns_config_windows.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace {

constexpr int kAndroidLogInfo = 4;
constexpr int kAndroidLogWarn = 5;

struct Elf64Phdr {
  std::uint32_t p_type;
  std::uint32_t p_flags;
  std::uint64_t p_offset;
  std::uint64_t p_vaddr;
  std::uint64_t p_paddr;
  std::uint64_t p_filesz;
  std::uint64_t p_memsz;
  std::uint64_t p_align;
};
struct Elf64Dyn {
  std::int64_t d_tag;
  std::uint64_t d_val;
};
struct Elf64Sym {
  std::uint32_t st_name;
  std::uint8_t st_info;
  std::uint8_t st_other;
  std::uint16_t st_shndx;
  std::uint64_t st_value;
  std::uint64_t st_size;
};
static_assert(sizeof(Elf64Phdr) == 56 && sizeof(Elf64Dyn) == 16 && sizeof(Elf64Sym) == 24,
              "ELF64 record sizes");

constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint32_t kPtDynamic = 2;
constexpr std::int64_t kDtNull = 0;
constexpr std::int64_t kDtHash = 4;
constexpr std::int64_t kDtStrtab = 5;
constexpr std::int64_t kDtSymtab = 6;
constexpr std::int64_t kDtStrsz = 10;
constexpr std::int64_t kDtGnuHash = 0x6ffffef5;
constexpr std::int64_t kDtVersym = 0x6ffffff0;
constexpr std::uint8_t kStbGlobal = 1;
constexpr std::uint8_t kStbWeak = 2;
constexpr std::uint8_t kStbGnuUnique = 10;
constexpr std::uint8_t kSttObject = 1;
constexpr std::uint8_t kSttFunc = 2;
constexpr std::uint8_t kSttTls = 6;
constexpr std::uint8_t kSttGnuIfunc = 10;
constexpr std::uint8_t kStvDefault = 0;
constexpr std::uint8_t kStvProtected = 3;
constexpr std::uint16_t kShnUndef = 0;
constexpr std::uint16_t kVersymHidden = 0x8000;

constexpr const char* kOwnerLibraries[] = {"libc.so", "libm.so", "libdl.so", "liblog.so"};

constexpr const char* kEtherNames[] = {"ether_aton", "ether_aton_r", "ether_ntoa", "ether_ntoa_r"};
struct HostOwnableFamily {
  const char* header;
  const char* const* names;
  std::size_t count;
};
constexpr HostOwnableFamily kHostOwnableFamilies[] = {
    {"<netinet/ether.h>", kEtherNames, sizeof(kEtherNames) / sizeof(kEtherNames[0])},
};

struct ModuleProbe {
  std::uintptr_t probe = 0;
  bool found = false;
  std::uint64_t load_bias = 0;
  const Elf64Phdr* phdr = nullptr;
  std::size_t phnum = 0;
};

int ELIXIR_SYSV FindModule(guest::DlPhdrInfo* info, std::size_t, void* data) noexcept {
  auto* probe = static_cast<ModuleProbe*>(data);
  const auto* phdr = static_cast<const Elf64Phdr*>(info->dlpi_phdr);
  if (phdr == nullptr) return 0;
  for (std::size_t i = 0; i < info->dlpi_phnum; ++i) {
    if (phdr[i].p_type != kPtLoad) continue;
    const std::uintptr_t start = static_cast<std::uintptr_t>(info->dlpi_addr + phdr[i].p_vaddr);
    if (probe->probe >= start && probe->probe - start < phdr[i].p_memsz) {
      probe->found = true;
      probe->load_bias = info->dlpi_addr;
      probe->phdr = phdr;
      probe->phnum = info->dlpi_phnum;
      return 1;
    }
  }
  return 0;
}

std::size_t GnuHashSymbolCount(const std::uint32_t* table) {
  const std::uint32_t nbuckets = table[0];
  const std::uint32_t symoffset = table[1];
  const std::uint32_t bloom_words = table[2];
  const auto* buckets = table + 4 + static_cast<std::size_t>(bloom_words) * 2;
  const auto* chain = buckets + nbuckets;
  std::uint32_t last = 0;
  for (std::uint32_t i = 0; i < nbuckets; ++i) last = std::max(last, buckets[i]);
  if (last < symoffset) return symoffset;
  while ((chain[last - symoffset] & 1u) == 0) ++last;
  return static_cast<std::size_t>(last) + 1;
}

struct ExportedSymbol {
  std::string name;
  std::uint8_t type;
};

bool ReadExportedSymbols(const ModuleProbe& module, std::vector<ExportedSymbol>* symbols, std::string* error) {
  const Elf64Dyn* dynamic = nullptr;
  for (std::size_t i = 0; i < module.phnum; ++i) {
    if (module.phdr[i].p_type == kPtDynamic) {
      dynamic = reinterpret_cast<const Elf64Dyn*>(static_cast<std::uintptr_t>(module.load_bias + module.phdr[i].p_vaddr));
    }
  }
  if (dynamic == nullptr) {
    *error = "the extension has no PT_DYNAMIC segment";
    return false;
  }
  const Elf64Sym* symtab = nullptr;
  const char* strtab = nullptr;
  std::uint64_t strsz = 0;
  const std::uint32_t* sysv_hash = nullptr;
  const std::uint32_t* gnu_hash = nullptr;
  const std::uint16_t* versym = nullptr;
  for (const Elf64Dyn* d = dynamic; d->d_tag != kDtNull; ++d) {
    const std::uintptr_t address = static_cast<std::uintptr_t>(module.load_bias + d->d_val);
    switch (d->d_tag) {
      case kDtSymtab: symtab = reinterpret_cast<const Elf64Sym*>(address); break;
      case kDtStrtab: strtab = reinterpret_cast<const char*>(address); break;
      case kDtStrsz: strsz = d->d_val; break;
      case kDtHash: sysv_hash = reinterpret_cast<const std::uint32_t*>(address); break;
      case kDtGnuHash: gnu_hash = reinterpret_cast<const std::uint32_t*>(address); break;
      case kDtVersym: versym = reinterpret_cast<const std::uint16_t*>(address); break;
      default: break;
    }
  }
  if (symtab == nullptr || strtab == nullptr || (sysv_hash == nullptr && gnu_hash == nullptr)) {
    *error = "the extension's dynamic section lacks DT_SYMTAB, DT_STRTAB or a hash table";
    return false;
  }
  const std::size_t count = sysv_hash != nullptr ? sysv_hash[1] : GnuHashSymbolCount(gnu_hash);
  for (std::size_t i = 1; i < count; ++i) {
    const Elf64Sym& sym = symtab[i];
    if (sym.st_shndx == kShnUndef || sym.st_name >= strsz) continue;
    const std::uint8_t bind = static_cast<std::uint8_t>(sym.st_info >> 4);
    const std::uint8_t type = static_cast<std::uint8_t>(sym.st_info & 0xf);
    const std::uint8_t visibility = static_cast<std::uint8_t>(sym.st_other & 0x3);
    if (bind != kStbGlobal && bind != kStbWeak && bind != kStbGnuUnique) continue;
    if (visibility != kStvDefault && visibility != kStvProtected) continue;
    if (versym != nullptr && (versym[i] & kVersymHidden) != 0) continue;
    const char* name = strtab + sym.st_name;
    if (*name == '\0') continue;
    if (type == kSttTls) {
      *error = std::string("the extension exports the TLS symbol ") + name +
               ", which a synthetic library cannot carry";
      return false;
    }
    if (type != kSttFunc && type != kSttObject && type != kSttGnuIfunc) {
      *error = std::string("the extension exports ") + name + " with unsupported symbol type " +
               std::to_string(type);
      return false;
    }
    symbols->push_back({name, type});
  }
  return true;
}

std::string JoinNames(const std::vector<std::string>& names) {
  constexpr std::size_t kShown = 12;
  std::string joined;
  for (std::size_t i = 0; i < names.size() && i < kShown; ++i) {
    if (i != 0) joined += ", ";
    joined += names[i];
  }
  if (names.size() > kShown) joined += ", ... (" + std::to_string(names.size()) + " in total)";
  return joined;
}

std::string LoaderError(const char* what) {
  const char* detail = linker::dlerror();
  return std::string(what) + (detail != nullptr ? std::string(": ") + detail : std::string());
}

bool ModuleContains(const ModuleProbe& module, std::uintptr_t address) {
  for (std::size_t i = 0; i < module.phnum; ++i) {
    if (module.phdr[i].p_type != kPtLoad) continue;
    const std::uintptr_t start = static_cast<std::uintptr_t>(module.load_bias + module.phdr[i].p_vaddr);
    if (address >= start && address - start < module.phdr[i].p_memsz) return true;
  }
  return false;
}

class LoadedLibrary {
 public:
  explicit LoadedLibrary(void* handle) : handle_(handle) {}
  ~LoadedLibrary() {
    if (handle_ != nullptr) linker::dlclose(handle_);
  }
  LoadedLibrary(const LoadedLibrary&) = delete;
  LoadedLibrary& operator=(const LoadedLibrary&) = delete;
  void* get() const { return handle_; }
  void Commit() { handle_ = nullptr; }

 private:
  void* handle_;
};

std::mutex g_mutex;
std::vector<std::string> g_loaded_paths;
std::vector<void*> g_loaded_handles;
std::vector<std::string> g_merged;

bool Contains(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

bool LoadLocked(const std::string& library_path, std::string* error) {
  void* libc = linker::dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD);
  if (libc == nullptr) {
    *error = "the synthetic libc.so is not installed (call InstallSyntheticLibraries() first)";
    return false;
  }
  LoadedLibrary extension(linker::dlopen(library_path.c_str(), RTLD_NOW));
  if (extension.get() == nullptr) {
    *error = LoaderError(("cannot load " + library_path).c_str());
    return false;
  }
  if (std::find(g_loaded_handles.begin(), g_loaded_handles.end(), extension.get()) != g_loaded_handles.end()) {
    return true;
  }
  auto refuse = [&](const std::string& message) {
    *error = library_path + ": " + message;
    return false;
  };

  std::size_t code_base = 0;
  std::size_t code_size = 0;
  linker::get_library_code_region(extension.get(), code_base, code_size);
  ModuleProbe module;
  module.probe = code_size != 0 ? code_base : linker::get_library_base(extension.get());
  linker::dl_iterate_phdr(
      reinterpret_cast<int(ELIXIR_GUEST_CC*)(struct dl_phdr_info*, std::size_t, void*)>(&FindModule), &module);
  if (!module.found) return refuse("the loader does not report the extension's segments");

  std::vector<ExportedSymbol> symbols;
  std::string scan_error;
  if (!ReadExportedSymbols(module, &symbols, &scan_error)) return refuse(scan_error);
  if (symbols.empty()) return refuse("the extension exports no symbols");

  std::vector<void*> owners;
  for (const char* soname : kOwnerLibraries) {
    if (void* owner = linker::dlopen(soname, RTLD_NOW | RTLD_NOLOAD)) owners.push_back(owner);
  }
  auto host_defines = [&](const char* name) {
    for (void* owner : owners) {
      if (linker::dlsym(owner, name) != nullptr) return true;
    }
    return false;
  };
  std::vector<std::string> exported;
  exported.reserve(symbols.size());
  for (const ExportedSymbol& symbol : symbols) exported.push_back(symbol.name);

  std::vector<std::string> yielded;
  for (const HostOwnableFamily& family : kHostOwnableFamilies) {
    std::vector<std::string> host_members;
    std::vector<std::string> extension_only;
    for (std::size_t i = 0; i < family.count; ++i) {
      const bool ours = Contains(exported, family.names[i]);
      if (host_defines(family.names[i])) {
        host_members.push_back(family.names[i]);
      } else if (ours) {
        extension_only.push_back(family.names[i]);
      }
    }
    if (host_members.empty()) continue;
    if (!extension_only.empty()) {
      return refuse(std::string("the host export table defines only part of ") + family.header + " (" +
                    JoinNames(host_members) + "); the extension would add " + JoinNames(extension_only));
    }
    for (const std::string& name : host_members) {
      if (Contains(exported, name)) yielded.push_back(name);
    }
  }

  std::vector<std::string> conflicts;
  std::unordered_map<std::string, void*> merged;
  merged.reserve(symbols.size());
  for (const ExportedSymbol& symbol : symbols) {
    if (Contains(yielded, symbol.name)) continue;
    if (host_defines(symbol.name.c_str())) {
      conflicts.push_back(symbol.name);
      continue;
    }
    void* address = linker::dlsym(extension.get(), symbol.name.c_str());
    if (address == nullptr) return refuse("cannot resolve its export " + symbol.name);
    if (!ModuleContains(module, reinterpret_cast<std::uintptr_t>(address))) {
      return refuse("its export " + symbol.name + " resolves outside the library's segments");
    }
    merged.emplace(symbol.name, address);
  }
  if (!conflicts.empty()) {
    return refuse("exports names the host export table already defines: " + JoinNames(conflicts));
  }
  if (!yielded.empty()) {
    HostLog(kAndroidLogWarn, "elixir",
            ("libc.so: " + JoinNames(yielded) + " come from the host export table; the copies in " + library_path +
             " are not merged")
                .c_str());
  }
  if (merged.empty()) {
    return true;
  }

  std::vector<std::string> names;
  names.reserve(merged.size());
  for (const auto& [name, address] : merged) names.push_back(name);
  std::vector<std::string> all_merged = g_merged;
  all_merged.insert(all_merged.end(), names.begin(), names.end());
  std::sort(all_merged.begin(), all_merged.end());
  g_loaded_paths.reserve(g_loaded_paths.size() + 1);
  g_loaded_handles.reserve(g_loaded_handles.size() + 1);

  linker::relocate(libc, merged);
  g_merged = std::move(all_merged);
  g_loaded_paths.push_back(library_path);
  g_loaded_handles.push_back(extension.get());
  extension.Commit();
  HostLog(kAndroidLogInfo, "elixir",
          ("libc.so: merged " + std::to_string(names.size()) + " symbols from " + library_path).c_str());
  return true;
}

}

bool LoadGuestLibcExtension(const std::string& library_path, std::string* error) {
  std::string ignored;
  if (error == nullptr) error = &ignored;
  bool loaded = false;
  bool resolver = false;
  try {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (Contains(g_loaded_paths, library_path)) return true;
    loaded = LoadLocked(library_path, error);
    resolver = loaded && std::binary_search(g_merged.begin(), g_merged.end(),
                                            std::string("_resolv_set_nameservers_for_net"));
  } catch (const std::exception& e) {
    *error = std::string("loading the libc extension failed: ") + e.what();
    return false;
  }
  if (resolver) dns_config::ConfigureGuestResolver();
  return loaded;
}

std::vector<std::string> GuestLibcExtensionExports() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_merged;
}

}
