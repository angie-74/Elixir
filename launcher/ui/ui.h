// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_UI_H_
#define ELIXIR_LAUNCHER_UI_UI_H_

#include <d2d1.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ui/renderer.h"

namespace elixir::launcher::ui {

enum class ControlKind {
  kButton, kNav, kToggle, kSegment, kSlider, kTextField, kListItem, kRadio, kLink, kStatic, kTextView, kComboBox
};
enum class ButtonStyle { kStandard, kSubtle, kDanger };

struct ControlRecord {
  std::wstring id;
  ControlKind kind = ControlKind::kButton;
  D2D1_RECT_F rect{};
  D2D1_RECT_F full_rect{};
  std::wstring name;
  std::wstring value;
  std::wstring description;
  std::wstring shortcut;
  bool enabled = true;
  bool checked = false;
  bool expanded = false;
  bool focusable = true;
  bool offscreen = false;
  int group = 0;
  double slider_min = 0, slider_max = 1, slider_step = 0.1, slider_value = 0;
};

struct Interaction {
  enum class Type { kNone, kActivate, kSetValue, kStep, kCommitText };
  Type type = Type::kNone;
  std::wstring id;
  double value = 0.0;
  int step = 0;
  std::wstring text;
};

enum class Ease {
  kOutCubic,
  kWpfDecelerate,
};

class Animator {
 public:
  void BeginFrame(double now_ms, bool enabled);
  bool EndFrame();
  float Value(std::wstring_view key, float target, float duration_ms, float initial, Ease ease);
  float Value(std::uint64_t key, float target, float duration_ms, float initial, Ease ease);
  static std::uint64_t Hash(std::wstring_view prefix, std::wstring_view id);
  void Finish(std::wstring_view key);

 private:
  struct Entry {
    float from = 0.0f;
    float to = 0.0f;
    double start = 0.0;
    float duration = 0.0f;
    Ease ease = Ease::kOutCubic;
    bool used = false;
  };
  float Current(const Entry& entry) const;

  std::unordered_map<std::uint64_t, Entry> entries_;
  double now_ = 0.0;
  bool enabled_ = true;
  bool moving_ = false;
};

inline constexpr std::uint32_t kDefaultControlAccentRgb = 0x4CC2FF;

struct UiState {
  std::wstring hovered;
  std::wstring pressed;
  std::wstring focused;
  std::wstring editing;
  bool focus_visible = false;
  float animation = 0.0f;
  Animator* animator = nullptr;
  std::uint32_t control_accent_rgb = kDefaultControlAccentRgb;
};

struct UiLayoutLog {
  struct TextBox {
    std::wstring text;
    D2D1_RECT_F rect;
    D2D1_RECT_F unclipped;
  };
  std::vector<TextBox> texts;
  std::vector<TextBox> trimmed;
};

class Ui {
 public:
  Ui(Renderer* renderer, const UiState* state, std::vector<ControlRecord>* records, const Interaction* interaction);

  bool painting() const { return interaction_ == nullptr; }
  Renderer& renderer() { return *renderer_; }
  const UiState& state() const { return *state_; }

  void PushClip(const D2D1_RECT_F& rect);
  void PopClip();
  void SetInert(bool inert) { inert_ = inert; }
  bool inert() const { return inert_; }
  void SetLayoutLog(UiLayoutLog* log) { layout_log_ = log; }

  float Animate(std::wstring_view key, float target, float duration_ms,
                float initial = std::numeric_limits<float>::quiet_NaN(), Ease ease = Ease::kOutCubic);
  float AnimateKeyed(const wchar_t* prefix, std::wstring_view id, float target, float duration_ms,
                     float initial = std::numeric_limits<float>::quiet_NaN(), Ease ease = Ease::kOutCubic);
  bool Culled(const D2D1_RECT_F& rect) const;
  void PushLayer(float opacity, float dy);
  void PopLayer();

  void Fill(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float radius = 0.0f);
  void Stroke(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float radius = 0.0f, float width = 1.0f);
  void Line(float x0, float y0, float x1, float y1, const D2D1_COLOR_F& color, float width = 1.0f);
  void FillGradient(const D2D1_RECT_F& rect, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float radius);
  float Text(float x, float y, float width, std::wstring_view text, TextStyle style, const D2D1_COLOR_F& color,
             Align align = Align::kLeading);
  void TextLine(const D2D1_RECT_F& rect, std::wstring_view text, TextStyle style, const D2D1_COLOR_F& color,
                Align align = Align::kLeading);
  float TextWidth(std::wstring_view text, TextStyle style);
  float TextHeight(std::wstring_view text, TextStyle style, float width);
  void Glyph(float center_x, float center_y, wchar_t glyph, TextStyle style, const D2D1_COLOR_F& color);
  void Logo(const D2D1_RECT_F& rect);
  void AppIcon(const D2D1_RECT_F& rect);
  void Image(const D2D1_RECT_F& rect, std::wstring_view path);
  void Progress(const D2D1_RECT_F& rect, float fraction);
  bool animating() const { return animating_; }

  int BeginCard(float x, float y, float width, const D2D1_COLOR_F& fill = {0, 0, 0, 0});
  float EndCard(int card, float content_bottom);

  void Label(const D2D1_RECT_F& rect, std::wstring_view name);
  void NativeRegion(std::wstring_view id, const D2D1_RECT_F& rect);

  bool Button(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label,
              ButtonStyle style = ButtonStyle::kStandard, bool enabled = true, wchar_t glyph = 0,
              std::wstring_view description = {});
  float ButtonWidth(std::wstring_view label, wchar_t glyph = 0, bool large = false);
  bool Nav(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label, bool selected,
           std::wstring_view shortcut);
  bool Toggle(std::wstring_view id, const D2D1_RECT_F& rect, bool* value, std::wstring_view name, bool enabled = true);
  bool ComboBox(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view name, std::wstring_view value,
                bool expanded, bool enabled);
  bool ComboOption(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label, bool selected);
  bool MenuRow(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view name);
  bool IconButton(std::wstring_view id, const D2D1_RECT_F& rect, wchar_t glyph, std::wstring_view name);
  bool Segmented(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
                 std::wstring_view name, bool enabled = true, float* out_width = nullptr);
  bool Tabs(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
            std::wstring_view name);
  bool Slider(std::wstring_view id, const D2D1_RECT_F& rect, double* value, double min, double max, double step,
              std::wstring_view name, std::wstring_view value_text, bool enabled = true, float value_width = 56.0f);
  bool CheckBox(std::wstring_view id, const D2D1_RECT_F& rect, bool* checked, std::wstring_view name,
                bool enabled = true);
  bool TextField(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring* text, std::wstring_view placeholder,
                 std::wstring_view name, bool enabled = true);
  bool ListItem(std::wstring_view id, const D2D1_RECT_F& rect, bool selected, std::wstring_view title,
                std::wstring_view subtitle, std::wstring_view trailing, wchar_t glyph, int group,
                int bitmap_resource = 0);
  bool Radio(std::wstring_view id, const D2D1_RECT_F& rect, bool selected, std::wstring_view label,
             std::wstring_view detail, int group);
  bool Link(std::wstring_view id, float x, float y, std::wstring_view text, float* out_width = nullptr,
            TextStyle style = TextStyle::kBody);
  bool TextView(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view text, float* scroll,
                std::wstring_view name);

  struct NativeRegionRecord {
    std::wstring id;
    D2D1_RECT_F rect;
  };
  const std::vector<NativeRegionRecord>& native_regions() const { return native_regions_; }
  const std::wstring& requested_focus() const { return requested_focus_; }

  void Flush();

 private:
  struct Command {
    enum class Type {
      kFill, kStroke, kLine, kGradient, kText, kBitmap, kIcon, kPushClip, kPopClip, kPushLayer, kPopLayer, kNone
    };
    Type type;
    D2D1_RECT_F rect;
    D2D1_COLOR_F color;
    D2D1_COLOR_F color2;
    float radius;
    float width;
    ComPtr<IDWriteTextLayout> layout;
    wchar_t icon = 0;
    int bitmap_resource = 0;
    std::wstring bitmap_file;
  };

  void Bitmap(const D2D1_RECT_F& rect, int resource_id);
  bool Hit(std::wstring_view id) const;
  bool Segments(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
                std::wstring_view name, bool enabled, float* out_width, bool tabs);
  ControlRecord* Record(std::wstring_view id, ControlKind kind, const D2D1_RECT_F& rect, std::wstring_view name,
                        bool enabled);
  void FocusRing(std::wstring_view id, const D2D1_RECT_F& rect, float radius);
  void LogText(std::wstring_view text, const D2D1_RECT_F& box);
  void TextViewLines(const D2D1_RECT_F& rect, std::wstring_view text, float scroll, float text_width,
                     float line_height);
  bool Hovered(std::wstring_view id) const { return state_->hovered == id; }
  bool Pressed(std::wstring_view id) const { return state_->pressed == id; }
  bool Focused(std::wstring_view id) const { return state_->focused == id; }

  Renderer* renderer_;
  const UiState* state_;
  std::vector<ControlRecord>* records_;
  const Interaction* interaction_;
  std::vector<Command> commands_;
  std::vector<D2D1_RECT_F> clips_;
  std::vector<float> clip_dy_;
  std::vector<float> layer_dys_;
  float layer_dy_ = 0.0f;
  std::vector<NativeRegionRecord> native_regions_;
  std::wstring requested_focus_;
  bool inert_ = false;
  bool animating_ = false;
  UiLayoutLog* layout_log_ = nullptr;
};

namespace glyph {
inline constexpr wchar_t kHome = 0xE80F;
inline constexpr wchar_t kPackage = 0xE7B8;
inline constexpr wchar_t kSettings = 0xE713;
inline constexpr wchar_t kDiagnostic = 0xE9D9;
inline constexpr wchar_t kBug = 0xEBE8;
inline constexpr wchar_t kSync = 0xE895;
inline constexpr wchar_t kInfo = 0xE946;
inline constexpr wchar_t kPlay = 0xE768;
inline constexpr wchar_t kFolder = 0xE8B7;
inline constexpr wchar_t kCopy = 0xE8C8;
inline constexpr wchar_t kSave = 0xE74E;
inline constexpr wchar_t kDelete = 0xE74D;
inline constexpr wchar_t kRefresh = 0xE72C;
inline constexpr wchar_t kCheck = 0xE73E;
inline constexpr wchar_t kCancel = 0xE711;
inline constexpr wchar_t kWarning = 0xE7BA;
inline constexpr wchar_t kOpenFile = 0xE8E5;
inline constexpr wchar_t kOpenExternal = 0xE8A7;
inline constexpr wchar_t kShield = 0xEA18;
inline constexpr wchar_t kDownload = 0xE896;
inline constexpr wchar_t kDocument = 0xE8A5;
inline constexpr wchar_t kAdd = 0xE710;
inline constexpr wchar_t kChevronRight = 0xE76C;
inline constexpr wchar_t kStop = 0xE71A;
inline constexpr wchar_t kFlag = 0xE7C1;
inline constexpr wchar_t kGame = 0xE7FC;
inline constexpr wchar_t kLink = 0xE71B;
inline constexpr wchar_t kUndo = 0xE7A7;
inline constexpr wchar_t kRedo = 0xE7A6;
inline constexpr wchar_t kImport = 0xE8B5;
inline constexpr wchar_t kExport = 0xEDE1;
inline constexpr wchar_t kBroom = 0xEA99;
inline constexpr wchar_t kGlobe = 0xE774;
inline constexpr wchar_t kSearch = 0xE721;
inline constexpr wchar_t kBack = 0xE72B;
inline constexpr wchar_t kReset = 0xE777;
inline constexpr wchar_t kChevronDown = 0xE70D;
inline constexpr wchar_t kEdit = 0xE70F;
inline constexpr wchar_t kEye = 0xE7B3;
inline constexpr wchar_t kEyeOff = 0xED1A;
inline constexpr wchar_t kPaste = 0xE77F;
inline constexpr wchar_t kMonitor = 0xE7F4;
inline constexpr wchar_t kWrench = 0xE90F;
}

}

#endif
