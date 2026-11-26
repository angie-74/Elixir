// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The SVG strings below are Lucide's icon files (ISC License, Copyright (c)
// 2026 Lucide Icons and Contributors; the Feather-derived ones MIT, Copyright
// (c) 2013-present Cole Bemis), copied unchanged from
// https://github.com/lucide-icons/lucide/tree/main/icons.
#include "ui/lucide.h"

#include <wrl/client.h>

#include <charconv>
#include <cmath>
#include <utility>
#include <vector>

#include "ui/ui.h"

namespace elixir::launcher::ui::lucide {
namespace {

using Microsoft::WRL::ComPtr;

struct Entry {
  wchar_t glyph;
  std::string_view name;
  std::string_view svg;
};

constexpr Entry kIcons[] = {
    {glyph::kHome, "house",
     R"(<path d="M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8" /> <path d="M3 10a2 2 0 0 1 .709-1.528l7-6a2 2 0 0 1 2.582 0l7 6A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" />)"},
    {glyph::kPackage, "package",
     R"(<path d="M11 21.73a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73z" /> <path d="M12 22V12" /> <polyline points="3.29 7 12 12 20.71 7" /> <path d="m7.5 4.27 9 5.15" />)"},
    {glyph::kSettings, "settings",
     R"(<path d="M9.671 4.136a2.34 2.34 0 0 1 4.659 0 2.34 2.34 0 0 0 3.319 1.915 2.34 2.34 0 0 1 2.33 4.033 2.34 2.34 0 0 0 0 3.831 2.34 2.34 0 0 1-2.33 4.033 2.34 2.34 0 0 0-3.319 1.915 2.34 2.34 0 0 1-4.659 0 2.34 2.34 0 0 0-3.32-1.915 2.34 2.34 0 0 1-2.33-4.033 2.34 2.34 0 0 0 0-3.831A2.34 2.34 0 0 1 6.35 6.051a2.34 2.34 0 0 0 3.319-1.915" /> <circle cx="12" cy="12" r="3" />)"},
    {glyph::kDiagnostic, "activity",
     R"(<path d="M22 12h-2.48a2 2 0 0 0-1.93 1.46l-2.35 8.36a.25.25 0 0 1-.48 0L9.24 2.18a.25.25 0 0 0-.48 0l-2.35 8.36A2 2 0 0 1 4.49 12H2" />)"},
    {glyph::kBug, "bug",
     R"(<path d="M12 20v-9" /> <path d="M14 7a4 4 0 0 1 4 4v3a6 6 0 0 1-12 0v-3a4 4 0 0 1 4-4z" /> <path d="M14.12 3.88 16 2" /> <path d="M21 21a4 4 0 0 0-3.81-4" /> <path d="M21 5a4 4 0 0 1-3.55 3.97" /> <path d="M22 13h-4" /> <path d="M3 21a4 4 0 0 1 3.81-4" /> <path d="M3 5a4 4 0 0 0 3.55 3.97" /> <path d="M6 13H2" /> <path d="m8 2 1.88 1.88" /> <path d="M9 7.13V6a3 3 0 1 1 6 0v1.13" />)"},
    {glyph::kSync, "refresh-cw",
     R"(<path d="M3 12a9 9 0 0 1 9-9 9.75 9.75 0 0 1 6.74 2.74L21 8" /> <path d="M21 3v5h-5" /> <path d="M21 12a9 9 0 0 1-9 9 9.75 9.75 0 0 1-6.74-2.74L3 16" /> <path d="M8 16H3v5" />)"},
    {glyph::kRefresh, "rotate-cw",
     R"(<path d="M21 12a9 9 0 1 1-9-9c2.52 0 4.93 1 6.74 2.74L21 8" /> <path d="M21 3v5h-5" />)"},
    {glyph::kReset, "rotate-ccw",
     R"(<path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" /> <path d="M3 3v5h5" />)"},
    {glyph::kInfo, "info", R"(<circle cx="12" cy="12" r="10" /> <path d="M12 16v-4" /> <path d="M12 8h.01" />)"},
    {glyph::kPlay, "play",
     R"(<path d="M5 5a2 2 0 0 1 3.008-1.728l11.997 6.998a2 2 0 0 1 .003 3.458l-12 7A2 2 0 0 1 5 19z" />)"},
    {glyph::kFolder, "folder-open",
     R"(<path d="m6 14 1.5-2.9A2 2 0 0 1 9.24 10H20a2 2 0 0 1 1.94 2.5l-1.54 6a2 2 0 0 1-1.95 1.5H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h3.9a2 2 0 0 1 1.69.9l.81 1.2a2 2 0 0 0 1.67.9H18a2 2 0 0 1 2 2v2" />)"},
    {glyph::kCopy, "copy",
     R"(<rect width="14" height="14" x="8" y="8" rx="2" ry="2" /> <path d="M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2" />)"},
    {glyph::kSave, "save",
     R"(<path d="M15.2 3a2 2 0 0 1 1.4.6l3.8 3.8a2 2 0 0 1 .6 1.4V19a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z" /> <path d="M17 21v-7a1 1 0 0 0-1-1H8a1 1 0 0 0-1 1v7" /> <path d="M7 3v4a1 1 0 0 0 1 1h7" />)"},
    {glyph::kDelete, "trash",
     R"(<path d="M10 11v6" /> <path d="M14 11v6" /> <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6" /> <path d="M3 6h18" /> <path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" />)"},
    {glyph::kCheck, "check", R"(<path d="M20 6 9 17l-5-5" />)"},
    {glyph::kCancel, "x", R"(<path d="M18 6 6 18" /> <path d="m6 6 12 12" />)"},
    {glyph::kWarning, "triangle-exclamation-point",
     R"(<path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3" /> <path d="M12 9v4" /> <path d="M12 17h.01" />)"},
    {glyph::kOpenFile, "file",
     R"(<path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z" /> <path d="M14 2v5a1 1 0 0 0 1 1h5" />)"},
    {glyph::kOpenExternal, "external-link",
     R"(<path d="M15 3h6v6" /> <path d="M10 14 21 3" /> <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6" />)"},
    {glyph::kShield, "shield-check",
     R"(<path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z" /> <path d="m9 12 2 2 4-4" />)"},
    {glyph::kDownload, "download",
     R"(<path d="M12 15V3" /> <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" /> <path d="m7 10 5 5 5-5" />)"},
    {glyph::kDocument, "file-text",
     R"(<path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z" /> <path d="M14 2v5a1 1 0 0 0 1 1h5" /> <path d="M10 9H8" /> <path d="M16 13H8" /> <path d="M16 17H8" />)"},
    {glyph::kAdd, "plus", R"(<path d="M5 12h14" /> <path d="M12 5v14" />)"},
    {glyph::kChevronRight, "chevron-right", R"(<path d="m9 18 6-6-6-6" />)"},
    {glyph::kChevronDown, "chevron-down", R"(<path d="m6 9 6 6 6-6" />)"},
    {glyph::kStop, "square", R"(<rect width="18" height="18" x="3" y="3" rx="2" />)"},
    {glyph::kFlag, "flag",
     R"(<path d="M4 22V4a1 1 0 0 1 .4-.8A6 6 0 0 1 8 2c3 0 5 2 7.333 2q2 0 3.067-.8A1 1 0 0 1 20 4v10a1 1 0 0 1-.4.8A6 6 0 0 1 16 16c-3 0-5-2-8-2a6 6 0 0 0-4 1.528" />)"},
    {glyph::kGame, "gamepad-2",
     R"(<line x1="6" x2="10" y1="11" y2="11" /> <line x1="8" x2="8" y1="9" y2="13" /> <line x1="15" x2="15.01" y1="12" y2="12" /> <line x1="18" x2="18.01" y1="10" y2="10" /> <path d="M17.32 5H6.68a4 4 0 0 0-3.978 3.59c-.006.052-.01.101-.017.152C2.604 9.416 2 14.456 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.414-1.414A2 2 0 0 1 9.828 16h4.344a2 2 0 0 1 1.414.586L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.545-.604-6.584-.685-7.258-.007-.05-.011-.1-.017-.151A4 4 0 0 0 17.32 5z" />)"},
    {glyph::kLink, "link",
     R"(<path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71" /> <path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71" />)"},
    {glyph::kUndo, "undo-2",
     R"(<path d="M9 14 4 9l5-5" /> <path d="M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5a5.5 5.5 0 0 1-5.5 5.5H11" />)"},
    {glyph::kRedo, "redo-2",
     R"(<path d="m15 14 5-5-5-5" /> <path d="M20 9H9.5A5.5 5.5 0 0 0 4 14.5A5.5 5.5 0 0 0 9.5 20H13" />)"},
    {glyph::kImport, "file-input",
     R"(<path d="M4 11V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.706.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-1" /> <path d="M14 2v5a1 1 0 0 0 1 1h5" /> <path d="M2 15h10" /> <path d="m9 18 3-3-3-3" />)"},
    {glyph::kExport, "file-output",
     R"(<path d="M4.226 20.925A2 2 0 0 0 6 22h12a2 2 0 0 0 2-2V8a2.4 2.4 0 0 0-.706-1.706l-3.588-3.588A2.4 2.4 0 0 0 14 2H6a2 2 0 0 0-2 2v3.127" /> <path d="M14 2v5a1 1 0 0 0 1 1h5" /> <path d="m5 11-3 3" /> <path d="m5 17-3-3h10" />)"},
    {glyph::kBroom, "brush-cleaning",
     R"(<path d="m16 22-1-4" /> <path d="M19 14a1 1 0 0 0 1-1v-1a2 2 0 0 0-2-2h-3a1 1 0 0 1-1-1V4a2 2 0 0 0-4 0v5a1 1 0 0 1-1 1H6a2 2 0 0 0-2 2v1a1 1 0 0 0 1 1" /> <path d="M19 14H5l-1.973 6.767A1 1 0 0 0 4 22h16a1 1 0 0 0 .973-1.233z" /> <path d="m8 22 1-4" />)"},
    {glyph::kGlobe, "globe",
     R"(<circle cx="12" cy="12" r="10" /> <path d="M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20" /> <path d="M2 12h20" />)"},
    {glyph::kSearch, "search", R"(<path d="m21 21-4.34-4.34" /> <circle cx="11" cy="11" r="8" />)"},
    {glyph::kBack, "arrow-left", R"(<path d="m12 19-7-7 7-7" /> <path d="M19 12H5" />)"},
    {glyph::kEdit, "square-pen",
     R"(<path d="M12 3H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7" /> <path d="M18.375 2.625a1 1 0 0 1 3 3l-9.013 9.014a2 2 0 0 1-.853.505l-2.873.84a.5.5 0 0 1-.62-.62l.84-2.873a2 2 0 0 1 .506-.852z" />)"},
    {glyph::kEye, "eye",
     R"(<path d="M2.062 12.348a1 1 0 0 1 0-.696 10.75 10.75 0 0 1 19.876 0 1 1 0 0 1 0 .696 10.75 10.75 0 0 1-19.876 0" /> <circle cx="12" cy="12" r="3" />)"},
    {glyph::kEyeOff, "eye-off",
     R"(<path d="M10.733 5.076a10.744 10.744 0 0 1 11.205 6.575 1 1 0 0 1 0 .696 10.747 10.747 0 0 1-1.444 2.49" /> <path d="M14.084 14.158a3 3 0 0 1-4.242-4.242" /> <path d="M17.479 17.499a10.75 10.75 0 0 1-15.417-5.151 1 1 0 0 1 0-.696 10.75 10.75 0 0 1 4.446-5.143" /> <path d="m2 2 20 20" />)"},
    {glyph::kPaste, "clipboard-paste",
     R"(<path d="M11 14h10" /> <path d="M16 4h2a2 2 0 0 1 2 2v1.344" /> <path d="m17 18 4-4-4-4" /> <path d="M8 4H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 1.793-1.113" /> <rect x="8" y="2" width="8" height="4" rx="1" />)"},
    {glyph::kMonitor, "monitor",
     R"(<rect width="20" height="14" x="2" y="3" rx="2" /> <line x1="8" x2="16" y1="21" y2="21" /> <line x1="12" x2="12" y1="17" y2="21" />)"},
    {glyph::kWrench, "wrench",
     R"(<path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.106-3.105c.32-.322.863-.22.983.218a6 6 0 0 1-8.259 7.057l-7.91 7.91a1 1 0 0 1-2.999-3l7.91-7.91a6 6 0 0 1 7.057-8.259c.438.12.54.662.219.984z" />)"},
};

bool IsSeparator(char c) { return c == ' ' || c == ',' || c == '\t' || c == '\r' || c == '\n'; }

void SkipSeparators(std::string_view text, std::size_t* at) {
  while (*at < text.size() && IsSeparator(text[*at])) ++*at;
}

bool ReadNumber(std::string_view text, std::size_t* at, float* value) {
  SkipSeparators(text, at);
  std::size_t end = *at;
  if (end < text.size() && (text[end] == '-' || text[end] == '+')) ++end;
  bool digits = false;
  bool point = false;
  while (end < text.size()) {
    const char c = text[end];
    if (c >= '0' && c <= '9') {
      digits = true;
    } else if (c == '.' && !point) {
      point = true;
    } else {
      break;
    }
    ++end;
  }
  if (!digits) return false;
  if (end < text.size() && (text[end] == 'e' || text[end] == 'E')) {
    std::size_t exponent = end + 1;
    if (exponent < text.size() && (text[exponent] == '-' || text[exponent] == '+')) ++exponent;
    if (exponent < text.size() && text[exponent] >= '0' && text[exponent] <= '9') {
      end = exponent;
      while (end < text.size() && text[end] >= '0' && text[end] <= '9') ++end;
    }
  }
  std::size_t first = *at;
  if (text[first] == '+') ++first;
  const auto result = std::from_chars(text.data() + first, text.data() + end, *value);
  if (result.ec != std::errc()) return false;
  *at = end;
  return true;
}

bool ReadFlag(std::string_view text, std::size_t* at, bool* flag) {
  SkipSeparators(text, at);
  if (*at >= text.size() || (text[*at] != '0' && text[*at] != '1')) return false;
  *flag = text[*at] == '1';
  ++*at;
  return true;
}

class Figures {
 public:
  explicit Figures(ID2D1GeometrySink* sink) : sink_(sink) {}

  void MoveTo(D2D1_POINT_2F point) {
    End();
    sink_->BeginFigure(point, D2D1_FIGURE_BEGIN_HOLLOW);
    open_ = true;
    start_ = current_ = point;
  }
  void LineTo(D2D1_POINT_2F point) {
    Begin();
    sink_->AddLine(point);
    current_ = point;
  }
  void CubicTo(D2D1_POINT_2F first, D2D1_POINT_2F second, D2D1_POINT_2F point) {
    Begin();
    sink_->AddBezier(D2D1::BezierSegment(first, second, point));
    current_ = point;
  }
  void QuadraticTo(D2D1_POINT_2F control, D2D1_POINT_2F point) {
    Begin();
    sink_->AddQuadraticBezier(D2D1::QuadraticBezierSegment(control, point));
    current_ = point;
  }
  void ArcTo(float rx, float ry, float rotation, bool large, bool sweep, D2D1_POINT_2F point) {
    if (rx == 0.0f || ry == 0.0f) {
      LineTo(point);
      return;
    }
    if (point.x == current_.x && point.y == current_.y) return;
    Begin();
    sink_->AddArc(D2D1::ArcSegment(point, D2D1::SizeF(std::fabs(rx), std::fabs(ry)), rotation,
                                   sweep ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                   large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    current_ = point;
  }
  void Close() {
    if (!open_) return;
    sink_->EndFigure(D2D1_FIGURE_END_CLOSED);
    open_ = false;
    current_ = start_;
  }
  void End() {
    if (!open_) return;
    sink_->EndFigure(D2D1_FIGURE_END_OPEN);
    open_ = false;
  }
  D2D1_POINT_2F current() const { return current_; }

  void StartElement() {
    End();
    start_ = current_ = D2D1::Point2F(0.0f, 0.0f);
  }

 private:
  void Begin() {
    if (open_) return;
    sink_->BeginFigure(current_, D2D1_FIGURE_BEGIN_HOLLOW);
    open_ = true;
    start_ = current_;
  }

  ID2D1GeometrySink* sink_;
  bool open_ = false;
  D2D1_POINT_2F start_{0.0f, 0.0f};
  D2D1_POINT_2F current_{0.0f, 0.0f};
};

D2D1_POINT_2F Reflect(D2D1_POINT_2F control, D2D1_POINT_2F about) {
  return D2D1::Point2F(2.0f * about.x - control.x, 2.0f * about.y - control.y);
}

bool AddPath(std::string_view data, Figures* figures) {
  std::size_t at = 0;
  char command = 0;
  char previous = 0;
  D2D1_POINT_2F control{0.0f, 0.0f};
  for (;;) {
    SkipSeparators(data, &at);
    if (at >= data.size()) return true;
    const char c = data[at];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      command = c;
      ++at;
    } else if (command == 0 || command == 'z' || command == 'Z') {
      return false;
    }
    const bool relative = command >= 'a';
    const D2D1_POINT_2F current = figures->current();
    const auto point = [&](float x, float y) {
      return relative ? D2D1::Point2F(current.x + x, current.y + y) : D2D1::Point2F(x, y);
    };
    float v[7] = {};
    const auto read = [&](int count) {
      for (int i = 0; i < count; ++i) {
        if (!ReadNumber(data, &at, &v[i])) return false;
      }
      return true;
    };
    const char kind = static_cast<char>(command | 0x20);
    switch (kind) {
      case 'm':
        if (!read(2)) return false;
        figures->MoveTo(point(v[0], v[1]));
        command = relative ? 'l' : 'L';
        break;
      case 'l':
        if (!read(2)) return false;
        figures->LineTo(point(v[0], v[1]));
        break;
      case 'h':
        if (!read(1)) return false;
        figures->LineTo(D2D1::Point2F(relative ? current.x + v[0] : v[0], current.y));
        break;
      case 'v':
        if (!read(1)) return false;
        figures->LineTo(D2D1::Point2F(current.x, relative ? current.y + v[0] : v[0]));
        break;
      case 'c': {
        if (!read(6)) return false;
        const D2D1_POINT_2F second = point(v[2], v[3]);
        figures->CubicTo(point(v[0], v[1]), second, point(v[4], v[5]));
        control = second;
        break;
      }
      case 's': {
        if (!read(4)) return false;
        const D2D1_POINT_2F first = previous == 'c' || previous == 's' ? Reflect(control, current) : current;
        const D2D1_POINT_2F second = point(v[0], v[1]);
        figures->CubicTo(first, second, point(v[2], v[3]));
        control = second;
        break;
      }
      case 'q': {
        if (!read(4)) return false;
        const D2D1_POINT_2F quad = point(v[0], v[1]);
        figures->QuadraticTo(quad, point(v[2], v[3]));
        control = quad;
        break;
      }
      case 't': {
        if (!read(2)) return false;
        const D2D1_POINT_2F quad = previous == 'q' || previous == 't' ? Reflect(control, current) : current;
        figures->QuadraticTo(quad, point(v[0], v[1]));
        control = quad;
        break;
      }
      case 'a': {
        bool large = false;
        bool sweep = false;
        if (!read(3) || !ReadFlag(data, &at, &large) || !ReadFlag(data, &at, &sweep) || !ReadNumber(data, &at, &v[3]) ||
            !ReadNumber(data, &at, &v[4])) {
          return false;
        }
        figures->ArcTo(v[0], v[1], v[2], large, sweep, point(v[3], v[4]));
        break;
      }
      case 'z':
        figures->Close();
        break;
      default:
        return false;
    }
    previous = kind;
  }
}

std::vector<std::pair<std::string_view, std::string_view>> Attributes(std::string_view element) {
  std::vector<std::pair<std::string_view, std::string_view>> attributes;
  std::size_t at = 0;
  for (;;) {
    const std::size_t equals = element.find('=', at);
    if (equals == std::string_view::npos) break;
    std::size_t name_end = equals;
    while (name_end > at && IsSeparator(element[name_end - 1])) --name_end;
    std::size_t name_start = name_end;
    while (name_start > at && !IsSeparator(element[name_start - 1])) --name_start;
    const std::size_t open = element.find('"', equals);
    if (open == std::string_view::npos) break;
    const std::size_t close = element.find('"', open + 1);
    if (close == std::string_view::npos) break;
    attributes.emplace_back(element.substr(name_start, name_end - name_start),
                            element.substr(open + 1, close - open - 1));
    at = close + 1;
  }
  return attributes;
}

bool Number(const std::vector<std::pair<std::string_view, std::string_view>>& attributes, std::string_view name,
            float* value) {
  for (const auto& [key, text] : attributes) {
    if (key != name) continue;
    std::size_t at = 0;
    return ReadNumber(text, &at, value);
  }
  return false;
}

std::string_view Text(const std::vector<std::pair<std::string_view, std::string_view>>& attributes,
                      std::string_view name) {
  for (const auto& [key, text] : attributes) {
    if (key == name) return text;
  }
  return {};
}

void AddEllipse(Figures* figures, float cx, float cy, float rx, float ry) {
  figures->MoveTo(D2D1::Point2F(cx + rx, cy));
  figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(cx - rx, cy));
  figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(cx + rx, cy));
  figures->Close();
}

bool AddPoints(std::string_view points, bool closed, Figures* figures) {
  std::size_t at = 0;
  bool first = true;
  float x = 0.0f;
  float y = 0.0f;
  while (ReadNumber(points, &at, &x)) {
    if (!ReadNumber(points, &at, &y)) return false;
    if (first) {
      figures->MoveTo(D2D1::Point2F(x, y));
      first = false;
    } else {
      figures->LineTo(D2D1::Point2F(x, y));
    }
  }
  if (first) return false;
  if (closed) {
    figures->Close();
  } else {
    figures->End();
  }
  return true;
}

bool AddElement(std::string_view tag, std::string_view element, Figures* figures) {
  const auto attributes = Attributes(element);
  figures->StartElement();
  if (tag == "path") {
    const bool ok = AddPath(Text(attributes, "d"), figures);
    figures->End();
    return ok;
  }
  if (tag == "circle" || tag == "ellipse") {
    float cx = 0.0f, cy = 0.0f, rx = 0.0f, ry = 0.0f;
    Number(attributes, "cx", &cx);
    Number(attributes, "cy", &cy);
    if (tag == "circle") {
      if (!Number(attributes, "r", &rx)) return false;
      ry = rx;
    } else if (!Number(attributes, "rx", &rx) || !Number(attributes, "ry", &ry)) {
      return false;
    }
    AddEllipse(figures, cx, cy, rx, ry);
    return true;
  }
  if (tag == "rect") {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f, rx = 0.0f, ry = 0.0f;
    Number(attributes, "x", &x);
    Number(attributes, "y", &y);
    if (!Number(attributes, "width", &width) || !Number(attributes, "height", &height)) return false;
    const bool has_rx = Number(attributes, "rx", &rx);
    const bool has_ry = Number(attributes, "ry", &ry);
    if (has_rx && !has_ry) ry = rx;
    if (has_ry && !has_rx) rx = ry;
    rx = std::fmin(rx, width / 2.0f);
    ry = std::fmin(ry, height / 2.0f);
    if (rx <= 0.0f || ry <= 0.0f) {
      figures->MoveTo(D2D1::Point2F(x, y));
      figures->LineTo(D2D1::Point2F(x + width, y));
      figures->LineTo(D2D1::Point2F(x + width, y + height));
      figures->LineTo(D2D1::Point2F(x, y + height));
    } else {
      figures->MoveTo(D2D1::Point2F(x + rx, y));
      figures->LineTo(D2D1::Point2F(x + width - rx, y));
      figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(x + width, y + ry));
      figures->LineTo(D2D1::Point2F(x + width, y + height - ry));
      figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(x + width - rx, y + height));
      figures->LineTo(D2D1::Point2F(x + rx, y + height));
      figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(x, y + height - ry));
      figures->LineTo(D2D1::Point2F(x, y + ry));
      figures->ArcTo(rx, ry, 0.0f, false, true, D2D1::Point2F(x + rx, y));
    }
    figures->Close();
    return true;
  }
  if (tag == "line") {
    float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
    Number(attributes, "x1", &x1);
    Number(attributes, "y1", &y1);
    Number(attributes, "x2", &x2);
    Number(attributes, "y2", &y2);
    figures->MoveTo(D2D1::Point2F(x1, y1));
    figures->LineTo(D2D1::Point2F(x2, y2));
    figures->End();
    return true;
  }
  if (tag == "polyline" || tag == "polygon") return AddPoints(Text(attributes, "points"), tag == "polygon", figures);
  return false;
}

}

Icon IconFor(wchar_t glyph) {
  for (const Entry& entry : kIcons) {
    if (entry.glyph == glyph) return Icon{entry.name, entry.svg};
  }
  return {};
}

bool BuildGeometry(ID2D1Factory* factory, std::string_view svg, ID2D1PathGeometry** geometry) {
  if (factory == nullptr || geometry == nullptr || svg.empty()) return false;
  *geometry = nullptr;
  ComPtr<ID2D1PathGeometry> path;
  ComPtr<ID2D1GeometrySink> sink;
  if (FAILED(factory->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) return false;
  Figures figures(sink.Get());
  bool ok = true;
  std::size_t at = 0;
  while (ok) {
    const std::size_t open = svg.find('<', at);
    if (open == std::string_view::npos) break;
    const std::size_t close = svg.find('>', open);
    if (close == std::string_view::npos) {
      ok = false;
      break;
    }
    const std::string_view element = svg.substr(open + 1, close - open - 1);
    std::size_t tag_end = 0;
    while (tag_end < element.size() && element[tag_end] >= 'a' && element[tag_end] <= 'z') ++tag_end;
    ok = AddElement(element.substr(0, tag_end), element.substr(tag_end), &figures);
    at = close + 1;
  }
  figures.End();
  if (FAILED(sink->Close()) || !ok) return false;
  *geometry = path.Detach();
  return true;
}

}
