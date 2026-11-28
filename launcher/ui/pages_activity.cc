// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/activity.h"
#include "core/i18n.h"
#include "core/util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

constexpr std::size_t kShownGames = 30;
constexpr ULONGLONG kDetailsEveryMs = 10 * 60 * 1000;
constexpr ULONGLONG kDetailsRetryMs = 20 * 1000;

std::wstring GameTitle(const ActivityGame& game) {
  return game.name.empty() ? std::to_wstring(game.place_id) : game.name;
}

std::wstring Joined(const std::vector<std::wstring>& parts) {
  std::wstring out;
  for (const std::wstring& part : parts) {
    if (part.empty()) continue;
    if (!out.empty()) out += L" \x00B7 ";
    out += part;
  }
  return out;
}

}

const SavedAccount* App::ActivityAccount(std::int64_t user_id) const {
  if (user_id <= 0) return nullptr;
  const std::wstring id = std::to_wstring(user_id);
  for (const SavedAccount& account : activity_.accounts) {
    if (account.id == id) return &account;
  }
  return nullptr;
}

void App::RefreshActivity() {
  if (activity_.loading) {
    activity_.again = true;
    return;
  }
  activity_.loading = true;
  const std::wstring folder = paths_.activity_dir;
  const std::wstring accounts_dir = paths_.accounts_dir;
  RunInBackground([this, folder, accounts_dir]() -> std::function<void()> {
    std::vector<ActivityGame> games = ReadActivity(folder, UnixNow());
    std::vector<SavedAccount> accounts = ListSavedAccounts(accounts_dir);
    return [this, games = std::move(games), accounts = std::move(accounts)]() mutable {
      activity_.games = std::move(games);
      activity_.accounts = std::move(accounts);
      activity_.loaded = true;
      activity_.loading = false;
      if (activity_.again) {
        activity_.again = false;
        RefreshActivity();
      } else {
        UpdateActivityDetailsNow(false);
      }
      Invalidate();
    };
  });
}

void App::UpdateActivityDetailsNow(bool force) {
  const HttpGetFunction get = WebGet();
  if (!get || activity_.updating || activity_.games.empty()) return;
  const bool missing = std::any_of(activity_.games.begin(), activity_.games.end(), [](const ActivityGame& game) {
    return game.name.empty() || game.icon_file.empty();
  });
  const ULONGLONG now = GetTickCount64();
  const ULONGLONG wait = missing ? kDetailsRetryMs : kDetailsEveryMs;
  if (!force && activity_.updated_at != 0 && now - activity_.updated_at < wait) return;
  activity_.updating = true;
  const std::wstring folder = paths_.activity_dir;
  RunInBackground([this, folder, get]() -> std::function<void()> {
    const int changed = UpdateActivityDetails(folder, get, UnixNow());
    return [this, changed] {
      activity_.updating = false;
      activity_.updated_at = GetTickCount64();
      if (changed > 0) RefreshActivity();
    };
  });
}

void App::JoinActivityGame(std::size_t index, bool same_server) {
  if (index >= activity_.games.size()) return;
  const ActivityGame game = activity_.games[index];
  LinkRequest request = ParseLink(ActivityJoinLink(game, same_server));
  if (request.kind != LinkRequestKind::kJoin) {
    ShowToast(T(L"Roblox couldn’t join that experience."));
    return;
  }
  if (!runtime_.running && game.user_id > 0 && !switching_ && config_.installed.present() &&
      PathIsFile(paths_.runtime_exe)) {
    const std::wstring id = std::to_wstring(game.user_id);
    if (id != SignedInAccountId(paths_.data_root)) {
      RefreshInstanceAccounts();
      for (const InstanceAccountRow& row : instance_accounts_) {
        if (row.account.id != id) continue;
        InstanceAccount account;
        account.sign_in = InstanceSignIn::kSavedAccount;
        account.session_file = row.account.session_file;
        DropPendingJoin();
        pending_join_ = PendingJoin{std::move(request.join), GetTickCount64()};
        PlayWith(account, id);
        return;
      }
    }
  }
  OnLinkRequest(std::move(request.join));
}

void App::RemoveActivityRow(std::size_t index) {
  if (index >= activity_.games.size()) return;
  const ActivityGame game = activity_.games[index];
  if (!RemoveActivityGame(paths_.activity_dir, game.place_id)) return;
  activity_.motion.leaving.clear();
  activity_.motion.Leave(std::to_wstring(game.place_id), index, GameTitle(game), game.icon_file);
  activity_.games.erase(activity_.games.begin() + static_cast<std::ptrdiff_t>(index));
  Invalidate();
}

void App::ClearActivityHistory() {
  if (!ClearActivity(paths_.activity_dir)) return;
  activity_.motion = ListMotion{};
  activity_.games.clear();
  Invalidate();
}

float App::BuildActivity(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Activity"), L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const std::int64_t now = UnixNow();
  const std::vector<ActivityGame>& games = activity_.games;
  const bool can_join = !switching_ && config_.installed.present();
  const float play_width = ui.ButtonWidth(T(L"Play"));
  const float rejoin_width = ui.ButtonWidth(T(L"Rejoin server"));

  const auto account_name = [this](const ActivityGame& game) -> std::wstring {
    const SavedAccount* account = ActivityAccount(game.user_id);
    return account != nullptr ? SavedAccountName(*account) : std::wstring();
  };
  const auto picture = [&ui](const D2D1_RECT_F& rect, const ActivityGame& game, float radius) {
    ui.Fill(rect, theme::White(0.06f), radius);
    if (game.icon_file.empty()) {
      ui.Glyph((rect.left + rect.right) / 2.0f, (rect.top + rect.bottom) / 2.0f + 0.5f, glyph::kGame,
               TextStyle::kIcon, theme::kTextSecondary);
    } else {
      ui.Image(rect, game.icon_file, true);
    }
  };

  if (games.empty()) {
    const int card = ui.BeginCard(x, y, width);
    const std::wstring& text = T(L"Games you play in Elixir show up here.");
    const float top = y + 20.0f;
    const float height = ui.Text(cx, top, cw, text, TextStyle::kBody, theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, top, cx + cw, top + height), text);
    y = ui.EndCard(card, top + height);
  } else {
    const ActivityGame& game = games[0];
    const std::wstring title = GameTitle(game);
    const int card = ui.BeginCard(x, y, width);
    const float top = y + 20.0f;
    const D2D1_RECT_F icon = D2D1::RectF(cx, top, cx + 96.0f, top + 96.0f);
    picture(icon, game, 8.0f);
    const float tx = icon.right + 18.0f;
    const float tw = std::max(120.0f, cx + cw - tx);
    float ty = top;
    ty += ui.Text(tx, ty, tw, title, TextStyle::kCardTitle, theme::kText);
    if (!game.creator.empty()) {
      ty += 2.0f + ui.Text(tx, ty + 2.0f, tw, TF(L"By {0}", game.creator), TextStyle::kCaption, theme::kTextTertiary);
    }
    if (!game.description.empty()) {
      const float line = ui.TextHeight(L"Ag", TextStyle::kSecondary, tw);
      const float shown =
          std::min(ui.TextHeight(game.description, TextStyle::kSecondary, tw), std::ceil(line * 3.0f));
      ui.PushClip(D2D1::RectF(tx, ty + 8.0f, tx + tw, ty + 8.0f + shown));
      ui.Text(tx, ty + 8.0f, tw, game.description, TextStyle::kSecondary, theme::kTextSecondary);
      ui.PopClip();
      ty += 8.0f + shown;
    }
    ui.Label(D2D1::RectF(cx, top, cx + cw, std::max(ty, icon.bottom)),
             game.description.empty() ? title : title + L". " + game.description);
    float cy = std::max(ty, icon.bottom) + 18.0f;

    struct Fact {
      const std::wstring* label;
      std::wstring value;
    };
    const Fact facts[] = {
        {&T(L"Last played"), game.playing ? T(L"Playing now") : ActivityDay(game.last_played, now)},
        {&T(L"Time played"), ActivityDuration(game.seconds)},
        {&T(L"Account"), account_name(game)},
    };
    const float column = std::floor(cw / 3.0f);
    float facts_height = 0.0f;
    for (std::size_t i = 0; i < std::size(facts); ++i) {
      const float fx = cx + column * static_cast<float>(i);
      const float fw = column - 12.0f;
      const std::wstring value = facts[i].value.empty() ? std::wstring(L"\x2014") : facts[i].value;
      float height = ui.Text(fx, cy, fw, *facts[i].label, TextStyle::kCaption, theme::kTextTertiary);
      height += 2.0f + ui.Text(fx, cy + height + 2.0f, fw, value, TextStyle::kBody, theme::kText);
      ui.Label(D2D1::RectF(fx, cy, fx + fw, cy + height), *facts[i].label + L": " + value);
      facts_height = std::max(facts_height, height);
    }
    cy += facts_height + 18.0f;

    float bx = cx;
    if (!game.job_id.empty()) {
      if (ui.Button(L"activity.rejoin#0", D2D1::RectF(bx, cy, bx + rejoin_width, cy + theme::kButtonHeight),
                    T(L"Rejoin server"), ButtonStyle::kStandard, can_join, 0,
                    TF(L"Rejoin the server you last played {0} on", title))) {
        Defer([this] { JoinActivityGame(0, true); });
      }
      bx += rejoin_width + 8.0f;
    }
    if (ui.Button(L"activity.play#0", D2D1::RectF(bx, cy, bx + play_width, cy + theme::kButtonHeight), T(L"Play"),
                  ButtonStyle::kStandard, can_join, 0, TF(L"Play {0}", title))) {
      Defer([this] { JoinActivityGame(0, false); });
    }
    y = ui.EndCard(card, cy + theme::kButtonHeight);
  }

  struct Line {
    int game = -1;
    const ListMotion::Row* ghost = nullptr;
  };
  std::vector<Line> lines;
  for (std::size_t i = 1; i < games.size() && i <= kShownGames; ++i) lines.push_back(Line{static_cast<int>(i), nullptr});
  if (ui.painting()) {
    for (const ListMotion::Row& ghost : activity_.motion.leaving) {
      const std::size_t at = ghost.index > 0 ? ghost.index - 1 : 0;
      lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(std::min(at, lines.size())), Line{-1, &ghost});
    }
  }
  if (!lines.empty()) {
    y += 24.0f;
    y = SectionTitle(ui, x, y, width, T(L"Recent"));
    const int card = ui.BeginCard(x, y, width);
    constexpr float kRowHeight = 60.0f;
    float cy = y + 8.0f;
    std::vector<std::uint32_t> ended;
    bool first = true;
    for (const Line& line : lines) {
      float shown = 1.0f;
      if (line.ghost != nullptr) {
        shown = ui.Animate(L"activity.out." + std::to_wstring(line.ghost->serial), 0.0f, theme::kListMs, 1.0f);
        if (shown <= 0.0f) ended.push_back(line.ghost->serial);
      }
      const bool moving = line.ghost != nullptr;
      if (moving) {
        ui.PushClip(D2D1::RectF(x, cy, x + width, cy + kRowHeight * shown));
        ui.PushLayer(shown, 0.0f);
      }
      if (!first) ui.Line(cx, cy, cx + cw, cy, theme::White(0.06f));
      first = false;
      const D2D1_RECT_F icon = D2D1::RectF(cx, cy + 10.0f, cx + 40.0f, cy + 50.0f);
      if (line.ghost != nullptr) {
        ui.Fill(icon, theme::White(0.06f), 6.0f);
        ui.Image(icon, line.ghost->detail);
        ui.TextLine(D2D1::RectF(icon.right + 14.0f, cy, cx + cw, cy + kRowHeight), line.ghost->text, TextStyle::kBody,
                    theme::kText);
        ui.PopLayer();
        ui.PopClip();
        cy += kRowHeight * shown;
        continue;
      }
      const std::size_t index = static_cast<std::size_t>(line.game);
      const ActivityGame& game = games[index];
      const std::wstring title = GameTitle(game);
      const std::wstring number = std::to_wstring(game.place_id);
      picture(icon, game, 6.0f);
      const bool rejoin = !game.job_id.empty();
      const float remove_size = 28.0f;
      const float buttons = play_width + (rejoin ? rejoin_width + 8.0f : 0.0f) + 8.0f + remove_size;
      const float text_left = icon.right + 14.0f;
      const float text_right = std::max(text_left + 80.0f, cx + cw - buttons - 14.0f);
      const std::wstring facts =
          Joined({game.playing ? T(L"Playing now") : ActivityDay(game.last_played, now),
                  ActivityDuration(game.seconds), account_name(game)});
      ui.TextLine(D2D1::RectF(text_left, cy + 10.0f, text_right, cy + 31.0f), title, TextStyle::kBody, theme::kText);
      ui.TextLine(D2D1::RectF(text_left, cy + 31.0f, text_right, cy + 50.0f), facts, TextStyle::kCaption,
                  theme::kTextTertiary);
      ui.Label(D2D1::RectF(cx, cy, text_right, cy + kRowHeight), facts.empty() ? title : title + L", " + facts);
      const float button_top = cy + (kRowHeight - theme::kButtonHeight) / 2.0f;
      float bx = cx + cw - buttons;
      if (rejoin) {
        if (ui.Button(L"activity.rejoin#" + number,
                      D2D1::RectF(bx, button_top, bx + rejoin_width, button_top + theme::kButtonHeight),
                      T(L"Rejoin server"), ButtonStyle::kSubtle, can_join, 0,
                      TF(L"Rejoin the server you last played {0} on", title))) {
          Defer([this, index] { JoinActivityGame(index, true); });
        }
        bx += rejoin_width + 8.0f;
      }
      if (ui.Button(L"activity.play#" + number,
                    D2D1::RectF(bx, button_top, bx + play_width, button_top + theme::kButtonHeight), T(L"Play"),
                    ButtonStyle::kSubtle, can_join, 0, TF(L"Play {0}", title))) {
        Defer([this, index] { JoinActivityGame(index, false); });
      }
      bx += play_width + 8.0f;
      const float remove_top = cy + (kRowHeight - remove_size) / 2.0f;
      if (ui.IconButton(L"activity.remove#" + number,
                        D2D1::RectF(bx, remove_top, bx + remove_size, remove_top + remove_size), glyph::kCancel,
                        TF(L"Remove {0} from Activity", title))) {
        Defer([this, index] { RemoveActivityRow(index); });
      }
      cy += kRowHeight;
    }
    if (ui.painting() && !ended.empty()) activity_.motion.Finish(ended);
    y = ui.EndCard(card, cy - 12.0f);
  }

  y += 24.0f;
  D2D1_RECT_F control{};
  BeginOptions();
  float cy = y;
  cy = SettingRow(ui, cx, cy, cw, T(L"Keep history"), T(L"Elixir remembers the games you play on this PC."),
                  ui.ToggleWidth(), 32.0f, &control, true);
  if (ui.Toggle(L"activity.history", control, &config_.integrations.activity_history, T(L"Keep history"))) {
    SaveConfig();
  }
  const float clear_width = ui.ButtonWidth(T(L"Clear"));
  cy = SettingRow(ui, cx, cy, cw, T(L"Clear history"), L"", clear_width, theme::kButtonHeight, &control, false);
  if (ui.Button(L"activity.clear", control, T(L"Clear"), ButtonStyle::kStandard, !games.empty(), 0,
                T(L"Clear history"))) {
    Defer([this] { ClearActivityHistory(); });
  }
  return EndOptions(cy);
}

}
