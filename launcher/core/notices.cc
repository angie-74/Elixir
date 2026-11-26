// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/notices.h"

#include <windows.h>

#include <utility>

namespace elixir::launcher {
namespace {

std::vector<std::wstring_view> SplitLines(std::wstring_view text) {
  std::vector<std::wstring_view> lines;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    std::size_t end = text.find(L'\n', begin);
    if (end == std::wstring_view::npos) end = text.size();
    std::wstring_view line = text.substr(begin, end - begin);
    if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);
    lines.push_back(line);
    begin = end + 1;
  }
  return lines;
}

bool IsRule(std::wstring_view line, wchar_t mark) {
  if (line.size() < 3) return false;
  for (const wchar_t c : line) {
    if (c != mark) return false;
  }
  return true;
}

bool IsQuote(std::wstring_view line) { return line.size() > 4 && line.substr(0, 4) == L"    "; }

std::wstring_view Trim(std::wstring_view line) {
  while (!line.empty() && (line.front() == L' ' || line.front() == L'\t')) line.remove_prefix(1);
  while (!line.empty() && (line.back() == L' ' || line.back() == L'\t')) line.remove_suffix(1);
  return line;
}

}

std::vector<NoticeBlock> ParseNotices(std::wstring_view text) {
  std::vector<NoticeBlock> blocks;
  const std::vector<std::wstring_view> lines = SplitLines(text);
  NoticeBlock current;
  bool open = false;
  const auto flush = [&] {
    if (open) blocks.push_back(std::move(current));
    current = NoticeBlock{};
    open = false;
  };
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::wstring_view line = lines[i];
    if (IsQuote(line)) {
      if (!open || current.kind != NoticeKind::kQuote) {
        flush();
        current.kind = NoticeKind::kQuote;
        open = true;
      } else {
        current.text.push_back(L'\n');
      }
      current.text.append(line.substr(4));
      continue;
    }
    const std::wstring_view trimmed = Trim(line);
    if (trimmed.empty()) {
      if (open && current.kind == NoticeKind::kQuote) {
        std::size_t next = i + 1;
        while (next < lines.size() && Trim(lines[next]).empty()) ++next;
        if (next < lines.size() && IsQuote(lines[next])) {
          current.text.push_back(L'\n');
          continue;
        }
      }
      flush();
      continue;
    }
    if (line.front() != L' ' && i + 1 < lines.size() && (IsRule(lines[i + 1], L'=') || IsRule(lines[i + 1], L'-'))) {
      flush();
      blocks.push_back({IsRule(lines[i + 1], L'=') ? NoticeKind::kTitle : NoticeKind::kComponent, std::wstring(trimmed)});
      ++i;
      continue;
    }
    if (line.size() > 2 && line.substr(0, 2) == L"- ") {
      flush();
      current.kind = NoticeKind::kBullet;
      current.text = std::wstring(Trim(line.substr(2)));
      open = true;
      continue;
    }
    if (open && current.kind != NoticeKind::kQuote) {
      current.text.push_back(L' ');
      current.text.append(trimmed);
      continue;
    }
    flush();
    current.kind = NoticeKind::kParagraph;
    current.text = std::wstring(trimmed);
    open = true;
  }
  flush();
  return blocks;
}

bool LoadResourceText(int id, std::string* text) {
  const HMODULE module = GetModuleHandleW(nullptr);
  const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
  if (resource == nullptr) return false;
  const HGLOBAL loaded = LoadResource(module, resource);
  const DWORD size = SizeofResource(module, resource);
  const void* data = loaded != nullptr ? LockResource(loaded) : nullptr;
  if (data == nullptr || size == 0) return false;
  text->assign(static_cast<const char*>(data), size);
  return true;
}

}
