// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/import_check.h"

#include <windows.h>

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

#include "elixir_runtime/elf_dynamic.h"
#include "elixir_runtime/guest_environment.h"
#include "elixir_runtime/text.h"

namespace elixir::runtime {
namespace {

bool IsFile(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

struct Node {
  ImportCheckLibrary summary;
  ElfDynamicInfo elf;
};

}

ImportCheckReport CheckImports(const std::filesystem::path& library,
                               const std::vector<std::filesystem::path>& search_path) {
  ImportCheckReport report;
  const auto& synthetic = SyntheticLibraryExports();
  std::vector<Node> nodes;
  std::map<std::string, std::size_t> index_of;
  std::deque<std::size_t> queue;

  {
    Node root;
    std::string error;
    if (!ReadElfDynamicFile(library, &root.elf, &error)) {
      report.error = NativeUtf8(library) + ": " + error;
      return report;
    }
    root.summary.soname = Utf8FromWide(library.filename().native());
    root.summary.kind = "guest";
    root.summary.path = library;
    root.summary.needed = root.elf.needed;
    index_of[root.summary.soname] = 0;
    if (!root.elf.soname.empty()) index_of[root.elf.soname] = 0;
    nodes.push_back(std::move(root));
    queue.push_back(0);
  }
  while (!queue.empty()) {
    const std::size_t current = queue.front();
    queue.pop_front();
    const std::vector<std::string> needed = nodes[current].elf.needed;
    for (const std::string& name : needed) {
      if (index_of.count(name) != 0) continue;
      Node node;
      node.summary.soname = name;
      if (synthetic.count(name) != 0) {
        node.summary.kind = "elixir";
      } else {
        std::filesystem::path found;
        for (const std::filesystem::path& directory : search_path) {
          if (!directory.empty() && IsFile(directory / PathFromUtf8(name))) {
            found = directory / PathFromUtf8(name);
            break;
          }
        }
        std::string error;
        if (!found.empty() && ReadElfDynamicFile(found, &node.elf, &error)) {
          node.summary.kind = "guest";
          node.summary.path = found;
          node.summary.needed = node.elf.needed;
        } else {
          node.summary.kind = "missing";
          node.summary.path = found;
          ++report.missing_libraries;
        }
      }
      index_of[name] = nodes.size();
      nodes.push_back(std::move(node));
      if (nodes.back().summary.kind == "guest") queue.push_back(nodes.size() - 1);
    }
  }

  std::unordered_set<std::string> provided;
  for (const Node& node : nodes) {
    if (node.summary.kind == "elixir") {
      const auto& names = synthetic.at(node.summary.soname);
      provided.insert(names.begin(), names.end());
    } else if (node.summary.kind == "guest") {
      for (const std::string& name : node.elf.ExportedNames()) provided.insert(name);
    }
  }
  const auto outside_hint = [&synthetic, &index_of](const std::string& symbol) -> std::string {
    std::vector<std::string> owners;
    for (const auto& [soname, names] : synthetic) {
      if (index_of.count(soname) == 0 && names.count(symbol) != 0) owners.push_back(soname);
    }
    if (owners.empty()) return {};
    std::sort(owners.begin(), owners.end());
    std::string hint = "provided by ";
    for (std::size_t i = 0; i < owners.size(); ++i) hint += (i ? ", " : "") + owners[i];
    return hint + ", which no library here lists in DT_NEEDED";
  };
  for (Node& node : nodes) {
    if (node.summary.kind != "guest") continue;
    std::set<std::string> seen;
    for (const ElfDynamicSymbol& symbol : node.elf.symbols) {
      if (!symbol.undefined() || symbol.name.empty() || symbol.binding == 0) continue;
      if (!seen.insert(symbol.name).second) continue;
      if (symbol.weak()) {
        ++node.summary.weak_imports;
        if (provided.count(symbol.name) == 0) {
          report.weak_unresolved.push_back({node.summary.soname, symbol.name, outside_hint(symbol.name)});
        }
        continue;
      }
      ++node.summary.imports;
      if (provided.count(symbol.name) == 0) {
        report.unresolved.push_back({node.summary.soname, symbol.name, outside_hint(symbol.name)});
      }
    }
  }
  for (Node& node : nodes) report.libraries.push_back(std::move(node.summary));
  return report;
}

std::string FormatImportCheckReport(const ImportCheckReport& report, const std::filesystem::path& library) {
  std::ostringstream out;
  out << "elixir-runtime: import check of " << NativeUtf8(library) << "\n";
  if (!report.error.empty()) {
    out << "error: " << report.error << "\n";
    return out.str();
  }
  const auto& synthetic = SyntheticLibraryExports();
  out << "libraries (load order):\n";
  for (const ImportCheckLibrary& entry : report.libraries) {
    out << "  " << entry.soname << "  ";
    if (entry.kind == "elixir") {
      out << "provided by Elixir (" << synthetic.at(entry.soname).size() << " symbols)";
    } else if (entry.kind == "guest") {
      out << "guest library " << NativeUtf8(entry.path) << " (" << entry.imports << " imports, "
          << entry.weak_imports << " weak)";
    } else {
      out << (entry.path.empty() ? "MISSING: not provided by Elixir and not in the library search path"
                                 : "MISSING: " + NativeUtf8(entry.path) + " is not a readable ELF library");
    }
    out << "\n";
  }
  std::map<std::string, std::vector<const UnresolvedImport*>> by_library;
  for (const UnresolvedImport& entry : report.unresolved) by_library[entry.library].push_back(&entry);
  out << "unresolved imports: " << report.unresolved.size() << "\n";
  for (const auto& [name, entries] : by_library) {
    out << "  " << name << ": " << entries.size() << "\n";
    for (const UnresolvedImport* entry : entries) {
      out << "    " << entry->symbol;
      if (!entry->hint.empty()) out << "  (" << entry->hint << ")";
      out << "\n";
    }
  }
  if (!report.weak_unresolved.empty()) {
    out << "weak imports with no definition (bound to null, as on Android): " << report.weak_unresolved.size()
        << "\n";
    for (const UnresolvedImport& entry : report.weak_unresolved) {
      out << "    " << entry.library << ": " << entry.symbol << "\n";
    }
  }
  for (const UnresolvedImport& entry : report.unresolved) {
    out << "UNRESOLVED " << entry.library << " " << entry.symbol << "\n";
  }
  for (const ImportCheckLibrary& entry : report.libraries) {
    if (entry.kind == "missing") out << "MISSING " << entry.soname << "\n";
  }
  out << "result: " << (report.ok() ? "every import resolves" : "unresolved imports remain") << " ("
      << report.unresolved.size() << " unresolved, " << report.missing_libraries << " missing libraries)\n";
  return out.str();
}

}
