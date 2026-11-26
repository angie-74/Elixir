// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_NOTICES_H_
#define ELIXIR_LAUNCHER_CORE_NOTICES_H_

#include <string>
#include <string_view>
#include <vector>

namespace elixir::launcher {

enum class NoticeKind { kTitle, kComponent, kBullet, kParagraph, kQuote };

struct NoticeBlock {
  NoticeKind kind = NoticeKind::kParagraph;
  std::wstring text;
};

std::vector<NoticeBlock> ParseNotices(std::wstring_view text);

bool LoadResourceText(int id, std::string* text);

}

#endif
