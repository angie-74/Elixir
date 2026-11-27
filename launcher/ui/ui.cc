// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "ui/ui.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "resource.h"
#include "ui/theme.h"
#include "core/i18n.h"

namespace elixir::launcher::ui {

namespace {

D2D1_RECT_F Inflate(const D2D1_RECT_F& rect, float amount) {
  return D2D1::RectF(rect.left - amount, rect.top - amount, rect.right + amount, rect.bottom + amount);
}

D2D1_RECT_F Intersect(const D2D1_RECT_F& a, const D2D1_RECT_F& b) {
  return D2D1::RectF(std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right),
                     std::min(a.bottom, b.bottom));
}

bool IsEmpty(const D2D1_RECT_F& rect) { return rect.right <= rect.left || rect.bottom <= rect.top; }

int GroupOf(std::wstring_view id) {
  return static_cast<int>(std::hash<std::wstring_view>{}(id) & 0x3FFFFFFF) + 1;
}

constexpr float kHoverMs = 83.0f;
constexpr float kPressMs = 83.0f;
constexpr float kToggleMs = 167.0f;
constexpr float kSelectMs = 167.0f;
constexpr float kThumbMs = 120.0f;

D2D1_COLOR_F Mix(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) {
  return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha) {
  color.a = alpha;
  return color;
}

float RelativeLuminance(const D2D1_COLOR_F& color) {
  const auto linear = [](float channel) {
    return channel <= 0.04045f ? channel / 12.92f : std::pow((channel + 0.055f) / 1.055f, 2.4f);
  };
  return 0.2126f * linear(color.r) + 0.7152f * linear(color.g) + 0.0722f * linear(color.b);
}

D2D1_COLOR_F ContrastingMarker(const D2D1_COLOR_F& fill) {
  return RelativeLuminance(fill) >= 0.17913f ? theme::Rgb(0x000000) : theme::White(1.0f);
}

D2D1_COLOR_F HoveredControlAccent(std::uint32_t rgb, float hover) {
  return Mix(theme::Rgb(rgb), theme::kField, 0.1f * hover);
}

D2D1_COLOR_F States(const D2D1_COLOR_F& base, const D2D1_COLOR_F& hover_color, const D2D1_COLOR_F& down_color,
                    float hover, float down) {
  return Mix(Mix(base, hover_color, hover), down_color, down);
}

std::wstring Key(const wchar_t* prefix, std::wstring_view id) {
  std::wstring key(prefix);
  key.append(id);
  return key;
}

}

void Animator::BeginFrame(double now_ms, bool enabled) {
  now_ = now_ms;
  enabled_ = enabled;
  moving_ = false;
}

bool Animator::EndFrame() {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (!it->second.used) {
      it = entries_.erase(it);
    } else {
      it->second.used = false;
      ++it;
    }
  }
  return moving_;
}

float Animator::Current(const Entry& entry) const {
  if (entry.duration <= 0.0f || now_ >= entry.start + entry.duration) return entry.to;
  const float t = std::clamp(static_cast<float>((now_ - entry.start) / entry.duration), 0.0f, 1.0f);
  float eased = 0.0f;
  if (entry.ease == Ease::kWpfDecelerate) {
    constexpr float d = 0.7f;
    constexpr float r = 2.0f / (2.0f - d);
    const float u = 1.0f - t;
    eased = t <= 1.0f - d ? r * t : 1.0f - r * u * u / (2.0f * d);
  } else {
    const float rest = 1.0f - t;
    eased = 1.0f - rest * rest * rest;
  }
  return entry.from + (entry.to - entry.from) * eased;
}

std::uint64_t Animator::Hash(std::wstring_view prefix, std::wstring_view id) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const wchar_t c : prefix) {
    hash ^= static_cast<std::uint16_t>(c);
    hash *= 1099511628211ull;
  }
  for (const wchar_t c : id) {
    hash ^= static_cast<std::uint16_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

void Animator::Finish(std::wstring_view key) {
  const auto found = entries_.find(Hash(L"", key));
  if (found == entries_.end()) return;
  found->second.from = found->second.to;
  found->second.start = now_ - found->second.duration;
}

float Animator::Value(std::wstring_view key, float target, float duration_ms, float initial, Ease ease) {
  return Value(Hash(L"", key), target, duration_ms, initial, ease);
}

float Animator::Value(std::uint64_t key, float target, float duration_ms, float initial, Ease ease) {
  auto [it, inserted] = entries_.try_emplace(key);
  Entry& entry = it->second;
  if (inserted) {
    entry.from = std::isnan(initial) ? target : initial;
    entry.to = target;
    entry.start = now_;
    entry.duration = duration_ms;
    entry.ease = ease;
  } else if (target != entry.to) {
    entry.from = Current(entry);
    entry.to = target;
    entry.start = now_;
    entry.duration = duration_ms;
    entry.ease = ease;
  }
  entry.used = true;
  if (!enabled_) {
    entry.from = entry.to;
    entry.start = now_ - entry.duration;
    return entry.to;
  }
  const float value = Current(entry);
  if (value != entry.to) moving_ = true;
  return value;
}

Ui::Ui(Renderer* renderer, const UiState* state, std::vector<ControlRecord>* records, const Interaction* interaction)
    : renderer_(renderer), state_(state), records_(records), interaction_(interaction) {}

bool Ui::Hit(std::wstring_view id) const { return interaction_ != nullptr && interaction_->id == id; }

float Ui::Animate(std::wstring_view key, float target, float duration_ms, float initial, Ease ease) {
  if (!painting() || state_->animator == nullptr) return target;
  return state_->animator->Value(Animator::Hash(L"", key), target, duration_ms, initial, ease);
}

float Ui::AnimateKeyed(const wchar_t* prefix, std::wstring_view id, float target, float duration_ms, float initial,
                       Ease ease) {
  if (!painting() || state_->animator == nullptr) return target;
  return state_->animator->Value(Animator::Hash(prefix, id), target, duration_ms, initial, ease);
}

bool Ui::Culled(const D2D1_RECT_F& r) const {
  if (!painting() || clips_.empty()) return false;
  const D2D1_RECT_F& c = clips_.back();
  const float dy = layer_dy_ - clip_dy_.back();
  return r.bottom + dy <= c.top || r.top + dy >= c.bottom || r.right <= c.left || r.left >= c.right;
}

void Ui::PushLayer(float opacity, float dy) {
  if (painting()) {
    commands_.push_back(Command{Command::Type::kPushLayer, {}, D2D1::ColorF(0, 0, 0, std::clamp(opacity, 0.0f, 1.0f)),
                                {}, dy, 0, nullptr});
    layer_dys_.push_back(dy);
    layer_dy_ += dy;
  }
}

void Ui::PopLayer() {
  if (painting()) {
    commands_.push_back(Command{Command::Type::kPopLayer, {}, {}, {}, 0, 0, nullptr});
    if (!layer_dys_.empty()) {
      layer_dy_ -= layer_dys_.back();
      layer_dys_.pop_back();
    }
  }
}

ControlRecord* Ui::Record(std::wstring_view id, ControlKind kind, const D2D1_RECT_F& rect, std::wstring_view name,
                          bool enabled) {
  if (!painting() || inert_ || records_ == nullptr) return nullptr;
  ControlRecord record;
  record.id = std::wstring(id);
  record.kind = kind;
  record.full_rect = rect;
  record.rect = clips_.empty() ? rect : Intersect(rect, clips_.back());
  record.offscreen = IsEmpty(record.rect);
  record.name = std::wstring(name);
  record.enabled = enabled;
  records_->push_back(std::move(record));
  return &records_->back();
}

void Ui::FocusRing(std::wstring_view id, const D2D1_RECT_F& rect, float radius) {
  if (state_->focus_visible && Focused(id) && !inert_) {
    Stroke(Inflate(rect, 3.0f), theme::kFocusRing, radius + 3.0f, 2.0f);
  }
}

void Ui::PushClip(const D2D1_RECT_F& rect) {
  clips_.push_back(clips_.empty() ? rect : Intersect(rect, clips_.back()));
  clip_dy_.push_back(layer_dy_);
  if (painting()) commands_.push_back(Command{Command::Type::kPushClip, clips_.back(), {}, {}, 0, 0, nullptr});
}

void Ui::PopClip() {
  if (clips_.empty()) return;
  clips_.pop_back();
  clip_dy_.pop_back();
  if (painting()) commands_.push_back(Command{Command::Type::kPopClip, {}, {}, {}, 0, 0, nullptr});
}

void Ui::Fill(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float radius) {
  if (painting() && color.a > 0 && !Culled(rect)) {
    commands_.push_back(Command{Command::Type::kFill, rect, color, {}, radius, 0, nullptr});
  }
}

void Ui::Stroke(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float radius, float width) {
  if (painting() && color.a > 0 && !Culled(rect)) {
    commands_.push_back(Command{Command::Type::kStroke, Inflate(rect, -width / 2.0f), color, {}, radius, width, nullptr});
  }
}

void Ui::Line(float x0, float y0, float x1, float y1, const D2D1_COLOR_F& color, float width) {
  if (painting() && !Culled(D2D1::RectF(std::min(x0, x1) - width, std::min(y0, y1) - width, std::max(x0, x1) + width,
                                        std::max(y0, y1) + width))) {
    commands_.push_back(Command{Command::Type::kLine, D2D1::RectF(x0, y0, x1, y1), color, {}, 0, width, nullptr});
  }
}

void Ui::FillGradient(const D2D1_RECT_F& rect, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float radius) {
  if (painting() && !Culled(rect)) {
    commands_.push_back(Command{Command::Type::kGradient, rect, from, to, radius, 0, nullptr});
  }
}

float Ui::Text(float x, float y, float width, std::wstring_view text, TextStyle style, const D2D1_COLOR_F& color,
               Align align) {
  if (text.empty()) return 0.0f;
  IDWriteTextLayout* layout = renderer_->Layout(text, style, width, true, align);
  if (layout == nullptr) return 0.0f;
  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  if (painting() && !Culled(D2D1::RectF(x, y, x + width, y + metrics.height))) {
    commands_.push_back(Command{Command::Type::kText, D2D1::RectF(x, y, x + width, y + metrics.height), color, {}, 0, 0,
                                ComPtr<IDWriteTextLayout>(layout)});
    LogText(text, D2D1::RectF(x + metrics.left, y + metrics.top, x + metrics.left + metrics.width,
                              y + metrics.top + metrics.height));
  }
  return metrics.height;
}

void Ui::LogText(std::wstring_view text, const D2D1_RECT_F& box) {
  if (layout_log_ == nullptr || inert_) return;
  const D2D1_RECT_F clipped = clips_.empty() ? box : Intersect(box, clips_.back());
  if (IsEmpty(clipped)) return;
  layout_log_->texts.push_back({std::wstring(text), clipped, box});
}

void Ui::TextLine(const D2D1_RECT_F& rect, std::wstring_view text, TextStyle style, const D2D1_COLOR_F& color,
                  Align align, float min_scale) {
  if (!painting() || text.empty()) return;
  const float width = rect.right - rect.left;
  if (width <= 1.0f || Culled(rect)) return;
  IDWriteTextLayout* layout = min_scale < 1.0f ? renderer_->FittedLayout(text, style, width, align, min_scale)
                                               : renderer_->Layout(text, style, width, false, align, true);
  if (layout == nullptr) return;
  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  const float y = rect.top + ((rect.bottom - rect.top) - metrics.height) / 2.0f;
  commands_.push_back(Command{Command::Type::kText, D2D1::RectF(rect.left, y, rect.right, y + metrics.height), color, {},
                              0, 0, ComPtr<IDWriteTextLayout>(layout)});
  if (layout_log_ != nullptr && !inert_) {
    const float natural = TextWidth(text, style) * std::min(min_scale, 1.0f);
    const float shown = std::min(natural, width);
    const float left = align == Align::kCenter     ? rect.left + (width - shown) / 2.0f
                       : align == Align::kTrailing ? rect.right - shown
                                                   : rect.left;
    const D2D1_RECT_F box = D2D1::RectF(left, y, left + shown, y + metrics.height);
    if (natural > width + 0.5f) layout_log_->trimmed.push_back({std::wstring(text), box, box});
    LogText(text, box);
  }
}

float Ui::TextWidth(std::wstring_view text, TextStyle style) {
  return renderer_->Measure(text, style, 0.0f, false).width;
}

float Ui::TextHeight(std::wstring_view text, TextStyle style, float width) {
  return renderer_->Measure(text, style, width, true).height;
}

float Ui::ToggleWidth() {
  const float label = std::max(TextWidth(T(L"On"), TextStyle::kBody), TextWidth(T(L"Off"), TextStyle::kBody));
  return std::max(96.0f, std::ceil(label) + 56.0f);
}

void Ui::Glyph(float center_x, float center_y, wchar_t glyph_code, TextStyle style, const D2D1_COLOR_F& color) {
  if (!painting() || Culled(D2D1::RectF(center_x - 24.0f, center_y - 24.0f, center_x + 24.0f, center_y + 24.0f))) return;
  if (renderer_->Icon(glyph_code) != nullptr) {
    const float size = style == TextStyle::kIconLarge ? 20.0f
                       : style == TextStyle::kIcon    ? 16.0f
                                                      : 12.0f;
    Command command{Command::Type::kIcon,
                    D2D1::RectF(center_x - size / 2.0f, center_y - size / 2.0f, center_x + size / 2.0f,
                                center_y + size / 2.0f),
                    color, {}, 0, 0, nullptr};
    command.icon = glyph_code;
    commands_.push_back(std::move(command));
    return;
  }
  const wchar_t text[2] = {glyph_code, 0};
  IDWriteTextLayout* layout = renderer_->Layout(std::wstring_view(text, 1), style, 0.0f, false);
  if (layout == nullptr) return;
  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  const float x = center_x - metrics.width / 2.0f;
  const float y = center_y - metrics.height / 2.0f;
  commands_.push_back(Command{Command::Type::kText, D2D1::RectF(x, y, x + metrics.width, y + metrics.height), color, {},
                              0, 0, ComPtr<IDWriteTextLayout>(layout)});
}

void Ui::Logo(const D2D1_RECT_F& rect) { Bitmap(rect, IDR_ELIXIR_LOGO_PNG); }

void Ui::AppIcon(const D2D1_RECT_F& rect) { Bitmap(rect, IDR_ELIXIR_ICON_PNG); }

void Ui::Bitmap(const D2D1_RECT_F& rect, int resource_id) {
  if (!painting()) return;
  Command command{Command::Type::kBitmap, rect, D2D1::ColorF(0, 0, 0, 1.0f), {}, 0, 0, nullptr};
  command.bitmap_resource = resource_id;
  commands_.push_back(std::move(command));
}

float Ui::Image(const D2D1_RECT_F& rect, std::wstring_view path, bool fade_in) {
  if (!painting() || path.empty()) return 0.0f;
  const float shown = AnimateKeyed(L"image:", path, 1.0f, theme::kImageFadeMs,
                                   fade_in ? 0.0f : std::numeric_limits<float>::quiet_NaN());
  if (Culled(rect)) return shown;
  Command command{Command::Type::kBitmap, rect, D2D1::ColorF(0, 0, 0, shown), {}, 0, 0, nullptr};
  command.bitmap_file = std::wstring(path);
  commands_.push_back(std::move(command));
  return shown;
}

void Ui::Progress(const D2D1_RECT_F& rect, float fraction) {
  if (!painting()) return;
  const float radius = (rect.bottom - rect.top) / 2.0f;
  Fill(rect, theme::White(0.12f), radius);
  const float width = rect.right - rect.left;
  if (fraction >= 0.0f) {
    const std::wstring place =
        std::to_wstring(std::lround(rect.left)) + L"," + std::to_wstring(std::lround(rect.top + layer_dy_));
    const float shown = AnimateKeyed(L"progress:", place, std::clamp(fraction, 0.0f, 1.0f), theme::kProgressMs);
    const float filled = shown * width;
    if (filled > 0.5f) Fill(D2D1::RectF(rect.left, rect.top, rect.left + std::max(filled, 2 * radius), rect.bottom),
                            theme::kSelectionFill, radius);
    return;
  }
  animating_ = true;
  const float phase = std::fmod(state_->animation * 0.8f, 1.0f);
  const float segment = width * 0.32f;
  const float start = rect.left - segment + phase * (width + segment);
  PushClip(rect);
  Fill(D2D1::RectF(start, rect.top, start + segment, rect.bottom), theme::kSelectionFill, radius);
  PopClip();
}

int Ui::BeginCard(float x, float y, float width, const D2D1_COLOR_F& fill) {
  if (!painting()) return -1;
  const D2D1_COLOR_F color = fill.a > 0 ? fill : theme::kCard;
  commands_.push_back(
      Command{Command::Type::kFill, D2D1::RectF(x, y, x + width, y), color, {}, theme::kCardRadius, 0, nullptr});
  commands_.push_back(Command{Command::Type::kStroke, D2D1::RectF(x + 0.5f, y + 0.5f, x + width - 0.5f, y),
                              theme::kCardBorder, {}, theme::kCardRadius, 1.0f, nullptr});
  return static_cast<int>(commands_.size()) - 2;
}

float Ui::EndCard(int card, float content_bottom) {
  const float bottom = content_bottom + theme::kCardPadding;
  if (card >= 0 && card + 1 < static_cast<int>(commands_.size())) {
    commands_[static_cast<std::size_t>(card)].rect.bottom = bottom;
    commands_[static_cast<std::size_t>(card) + 1].rect.bottom = bottom - 0.5f;
    if (Culled(commands_[static_cast<std::size_t>(card)].rect)) {
      commands_[static_cast<std::size_t>(card)].type = Command::Type::kNone;
      commands_[static_cast<std::size_t>(card) + 1].type = Command::Type::kNone;
    }
  }
  return bottom;
}

void Ui::Label(const D2D1_RECT_F& rect, std::wstring_view name) {
  if (ControlRecord* record = Record(L"", ControlKind::kStatic, rect, name, true)) record->focusable = false;
}

void Ui::NativeRegion(std::wstring_view id, const D2D1_RECT_F& rect) {
  if (painting() && !inert_) {
    native_regions_.push_back({std::wstring(id), clips_.empty() ? rect : Intersect(rect, clips_.back())});
  }
}

float Ui::ButtonWidth(std::wstring_view label, wchar_t , bool large) {
  const float text = TextWidth(label, large ? TextStyle::kButtonLarge : TextStyle::kButton);
  return std::max(std::ceil(text + (large ? 44.0f : 32.0f)), 84.0f);
}

bool Ui::Button(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label, ButtonStyle style,
                bool enabled, wchar_t , std::wstring_view description) {
  if (!painting()) return enabled && Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kButton, rect, label, enabled)) {
    record->description = std::wstring(description);
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = enabled && Hovered(id);
  const bool down = hover && Pressed(id);
  const float h = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  const float d = AnimateKeyed(L"press:", id, down ? 1.0f : 0.0f, kPressMs);
  const float radius = theme::kControlRadius;
  const bool large = rect.bottom - rect.top >= 42.0f;
  D2D1_COLOR_F text_color = enabled ? theme::kText : theme::kTextDisabled;
  switch (style) {
    case ButtonStyle::kStandard:
    case ButtonStyle::kDanger:
      Fill(rect, States(theme::kSubtleFill, theme::kSubtleHover, theme::kSubtlePressed, h, d), radius);
      Stroke(rect, theme::kControlBorder, radius);
      if (style == ButtonStyle::kDanger && enabled) text_color = theme::kFail;
      break;
    case ButtonStyle::kSubtle: {
      const D2D1_COLOR_F fill = Mix(theme::kSubtleFill, theme::kSubtlePressed, d);
      if (h > 0.0f) Fill(rect, WithAlpha(fill, fill.a * h), radius);
      break;
    }
  }
  const TextStyle text_style = large ? TextStyle::kButtonLarge : TextStyle::kButton;
  const float text_width = std::min(TextWidth(label, text_style), rect.right - rect.left - 16.0f);
  const float x = (rect.left + rect.right) / 2.0f - text_width / 2.0f;
  TextLine(D2D1::RectF(x, rect.top, x + text_width + 1.0f, rect.bottom), label, text_style, text_color);
  FocusRing(id, rect, radius);
  return false;
}

bool Ui::Nav(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label, bool selected,
             std::wstring_view shortcut, bool draw_selection) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kNav, rect, label, true)) {
    record->checked = selected;
    record->shortcut = std::wstring(shortcut);
  }
  const bool hover = Hovered(id);
  const float sel = AnimateKeyed(L"select:", id, selected ? 1.0f : 0.0f, kSelectMs);
  const float h = AnimateKeyed(L"hover:", id, hover && !selected ? 1.0f : 0.0f, kHoverMs);
  const float d = AnimateKeyed(L"press:", id, hover && !selected && Pressed(id) ? 1.0f : 0.0f, kPressMs);
  const D2D1_COLOR_F hover_fill = Mix(theme::kNavHover, theme::kSubtlePressed, d);
  const D2D1_COLOR_F idle_fill = Mix(WithAlpha(hover_fill, 0.0f), hover_fill, h);
  const D2D1_COLOR_F fill = draw_selection ? Mix(idle_fill, theme::kNavSelected, sel) : idle_fill;
  if (fill.a > 0.0f) Fill(rect, fill, theme::kControlRadius);
  TextLine(D2D1::RectF(rect.left + 16.0f, rect.top, rect.right - 8.0f, rect.bottom), label, TextStyle::kBody,
           Mix(theme::kTextSecondary, theme::kText, sel));
  FocusRing(id, rect, theme::kControlRadius);
  return false;
}

bool Ui::Toggle(std::wstring_view id, const D2D1_RECT_F& rect, bool* value, std::wstring_view name, bool enabled) {
  if (!painting()) {
    if (enabled && Hit(id) && interaction_->type == Interaction::Type::kActivate) {
      *value = !*value;
      return true;
    }
    return false;
  }
  if (ControlRecord* record = Record(id, ControlKind::kToggle, rect, name, enabled)) {
    record->checked = *value;
    record->value = *value ? T(L"On") : T(L"Off");
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = enabled && Hovered(id);
  const float on = AnimateKeyed(L"toggle:", id, *value ? 1.0f : 0.0f, kToggleMs);
  const float h = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  const float center_y = (rect.top + rect.bottom) / 2.0f;
  const D2D1_RECT_F pill = D2D1::RectF(rect.right - 40.0f, center_y - 10.0f, rect.right, center_y + 10.0f);
  D2D1_COLOR_F on_fill = theme::kSelectionDisabled;
  D2D1_COLOR_F knob_on = theme::White(0.6f);
  if (enabled && on > 0.0f) {
    on_fill = HoveredControlAccent(state_->control_accent_rgb, h);
    knob_on = ContrastingMarker(on_fill);
  }
  const D2D1_COLOR_F off_fill = theme::White(0.05f * h);
  const D2D1_COLOR_F track = Mix(off_fill, on_fill, on);
  if (track.a > 0.0f) Fill(pill, track, 10.0f);
  const D2D1_COLOR_F outline = enabled ? theme::White(0.62f) : theme::White(0.25f);
  if (on < 1.0f) Stroke(pill, WithAlpha(outline, outline.a * (1.0f - on)), 10.0f, 1.0f);
  const float knob_x = (pill.left + 10.0f) + ((pill.right - 10.0f) - (pill.left + 10.0f)) * on;
  const float knob_r = 5.0f + 1.0f * on;
  const D2D1_COLOR_F knob_off = enabled ? theme::kTextSecondary : theme::kTextDisabled;
  Fill(D2D1::RectF(knob_x - knob_r, center_y - knob_r, knob_x + knob_r, center_y + knob_r), Mix(knob_off, knob_on, on),
       knob_r);
  TextLine(D2D1::RectF(rect.left, rect.top, pill.left - 12.0f, rect.bottom), *value ? T(L"On") : T(L"Off"),
           TextStyle::kBody, enabled ? theme::kText : theme::kTextDisabled, Align::kTrailing);
  FocusRing(id, pill, 10.0f);
  return false;
}

bool Ui::ComboBox(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view name, std::wstring_view value,
                  bool expanded, bool enabled) {
  if (!painting()) return enabled && Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kComboBox, rect, name, enabled)) {
    record->value = value;
    record->expanded = expanded;
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = enabled && Hovered(id);
  const float h = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  const float d = AnimateKeyed(L"press:", id, hover && Pressed(id) ? 1.0f : 0.0f, kPressMs);
  const float open = AnimateKeyed(L"open:", id, expanded ? 1.0f : 0.0f, kSelectMs);
  Fill(rect, States(theme::kSubtleFill, theme::kSubtleHover, theme::kSubtlePressed, h, d), theme::kControlRadius);
  Stroke(rect, theme::kControlBorder, theme::kControlRadius);
  TextLine(D2D1::RectF(rect.left + 12.0f, rect.top, rect.right - 36.0f, rect.bottom), value,
           TextStyle::kButton, enabled ? theme::kText : theme::kTextDisabled);
  Glyph(rect.right - 16.0f, (rect.top + rect.bottom) / 2.0f + 2.0f * std::max(d, open), glyph::kChevronDown,
        TextStyle::kIconSmall, enabled ? theme::Rgb(state_->control_accent_rgb) : theme::kTextDisabled);
  FocusRing(id, rect, theme::kControlRadius);
  return false;
}

bool Ui::ComboOption(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view label, bool selected) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kListItem, rect, label, true)) record->checked = selected;
  if (Culled(rect)) return false;
  const float h = AnimateKeyed(L"hover:", id, Hovered(id) || (Focused(id) && state_->focus_visible) ? 1.0f : 0.0f,
                               kHoverMs);
  if (h > 0.0f) Fill(rect, WithAlpha(theme::kNavSelected, theme::kNavSelected.a * h), theme::kControlRadius);
  if (selected) {
    Glyph(rect.left + 15.0f, (rect.top + rect.bottom) / 2.0f, glyph::kCheck, TextStyle::kIcon,
          theme::Rgb(state_->control_accent_rgb));
  }
  TextLine(D2D1::RectF(rect.left + 33.0f, rect.top, rect.right - 10.0f, rect.bottom), label,
           TextStyle::kBody, theme::kText);
  FocusRing(id, Inflate(rect, -3.0f), theme::kControlRadius);
  return false;
}

bool Ui::MenuRow(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view name) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  Record(id, ControlKind::kListItem, rect, name, true);
  if (Culled(rect)) return false;
  const float h = AnimateKeyed(L"hover:", id, Hovered(id) || (Focused(id) && state_->focus_visible) ? 1.0f : 0.0f,
                               kHoverMs);
  if (h > 0.0f) Fill(rect, WithAlpha(theme::kNavSelected, theme::kNavSelected.a * h), theme::kControlRadius);
  FocusRing(id, Inflate(rect, -3.0f), theme::kControlRadius);
  return false;
}

bool Ui::IconButton(std::wstring_view id, const D2D1_RECT_F& rect, wchar_t glyph, std::wstring_view name) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  Record(id, ControlKind::kButton, rect, name, true);
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = Hovered(id);
  const float h = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  const float d = AnimateKeyed(L"press:", id, hover && Pressed(id) ? 1.0f : 0.0f, kPressMs);
  if (h > 0.0f) {
    const D2D1_COLOR_F fill = Mix(theme::kSubtleHover, theme::kSubtlePressed, d);
    Fill(rect, WithAlpha(fill, fill.a * h), theme::kControlRadius);
  }
  Glyph((rect.left + rect.right) / 2.0f, (rect.top + rect.bottom) / 2.0f, glyph, TextStyle::kIconSmall,
        Mix(theme::kTextSecondary, theme::kText, h));
  FocusRing(id, rect, theme::kControlRadius);
  return false;
}

bool Ui::Segmented(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
                   std::wstring_view name, bool enabled, float* out_width) {
  return Segments(id, x, y, options, index, name, enabled, out_width, false);
}

bool Ui::Tabs(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
              std::wstring_view name) {
  return Segments(id, x, y, options, index, name, true, nullptr, true);
}

bool Ui::Segments(std::wstring_view id, float x, float y, const std::vector<std::wstring>& options, int* index,
                  std::wstring_view name, bool enabled, float* out_width, bool tabs) {
  const float height = 32.0f;
  const float inset = tabs ? 0.0f : 2.0f;
  const float gap = tabs ? 4.0f : 0.0f;
  const TextStyle text_style = tabs ? TextStyle::kBody : TextStyle::kButton;
  const float cell_radius = tabs ? theme::kControlRadius : 4.0f;
  std::vector<float> widths;
  std::vector<float> lefts;
  float cursor = x + inset;
  for (const std::wstring& option : options) {
    widths.push_back(std::ceil(TextWidth(option, text_style) + (tabs ? 28.0f : 26.0f)));
    lefts.push_back(cursor);
    cursor += widths.back() + gap;
  }
  const float total = options.empty() ? 2.0f * inset : cursor - gap + inset - x;
  if (out_width != nullptr) *out_width = total;
  const int group = GroupOf(id);
  const auto cell_of = [&](std::size_t i) {
    return D2D1::RectF(lefts[i], y + inset, lefts[i] + widths[i], y + height - inset);
  };
  if (!painting()) {
    for (std::size_t i = 0; i < options.size(); ++i) {
      std::wstring sub(id);
      sub += L"#" + std::to_wstring(i);
      if (!enabled || !Hit(sub)) continue;
      int target = *index;
      if (interaction_->type == Interaction::Type::kActivate) target = static_cast<int>(i);
      if (interaction_->type == Interaction::Type::kStep) {
        target = std::clamp(static_cast<int>(i) + interaction_->step, 0, static_cast<int>(options.size()) - 1);
        requested_focus_ = std::wstring(id) + L"#" + std::to_wstring(target);
      }
      if (target != *index) {
        *index = target;
        return true;
      }
      return false;
    }
    return false;
  }
  const D2D1_RECT_F outer = D2D1::RectF(x, y, x + total, y + height);
  if (Culled(Inflate(outer, 4.0f))) {
    for (std::size_t i = 0; i < options.size(); ++i) {
      std::wstring sub(id);
      sub += L"#" + std::to_wstring(i);
      if (ControlRecord* record = Record(sub, ControlKind::kSegment, cell_of(i), options[i], enabled)) {
        record->checked = static_cast<int>(i) == *index;
        record->group = group;
        record->description = std::wstring(name);
      }
    }
    return false;
  }
  if (!tabs) {
    Fill(outer, theme::kSubtleFill, theme::kControlRadius);
    Stroke(outer, theme::kControlBorder, theme::kControlRadius);
  }
  if (*index >= 0 && *index < static_cast<int>(widths.size())) {
    const std::size_t chosen = static_cast<std::size_t>(*index);
    const float shown_left = AnimateKeyed(L"segment.left:", id, lefts[chosen], kSelectMs);
    const float shown_right = AnimateKeyed(L"segment.right:", id, lefts[chosen] + widths[chosen], kSelectMs);
    const D2D1_RECT_F highlight = D2D1::RectF(shown_left, y + inset, shown_right, y + height - inset);
    if (tabs) {
      Fill(highlight, theme::kNavSelected, cell_radius);
    } else {
      std::wstring selected_sub(id);
      selected_sub += L"#" + std::to_wstring(*index);
      const float h = AnimateKeyed(L"hover:", selected_sub, enabled && Hovered(selected_sub) ? 1.0f : 0.0f, kHoverMs);
      Fill(highlight, enabled ? Mix(theme::kSelectionFill, theme::kSelectionHover, h) : theme::kSelectionDisabled,
           cell_radius);
    }
  }
  for (std::size_t i = 0; i < options.size(); ++i) {
    std::wstring sub(id);
    sub += L"#" + std::to_wstring(i);
    const D2D1_RECT_F cell = cell_of(i);
    const bool selected = static_cast<int>(i) == *index;
    if (ControlRecord* record = Record(sub, ControlKind::kSegment, cell, options[i], enabled)) {
      record->checked = selected;
      record->group = group;
      record->description = std::wstring(name);
    }
    if (!selected) {
      const bool hover = enabled && Hovered(sub);
      const float h = AnimateKeyed(L"hover:", sub, hover ? 1.0f : 0.0f, kHoverMs);
      if (h > 0.0f) {
        Fill(cell,
             tabs ? WithAlpha(theme::kNavHover, theme::kNavHover.a * h)
                  : theme::White((Pressed(sub) && hover ? 0.04f : 0.07f) * h),
             cell_radius);
      }
    }
    D2D1_COLOR_F label{};
    if (tabs) {
      const float sel = AnimateKeyed(L"select:", sub, selected ? 1.0f : 0.0f, kSelectMs);
      label = Mix(theme::kTextSecondary, theme::kText, sel);
    } else {
      label = !enabled ? (selected ? theme::White(0.62f) : theme::kTextDisabled)
              : selected ? theme::kTextOnSelection
                         : theme::kTextSecondary;
    }
    TextLine(cell, options[i], text_style, label, Align::kCenter);
    FocusRing(sub, cell, cell_radius);
  }
  return false;
}

bool Ui::Slider(std::wstring_view id, const D2D1_RECT_F& rect, double* value, double min, double max, double step,
                std::wstring_view name, std::wstring_view value_text, bool enabled, float value_width) {
  const auto snap = [&](double v) {
    v = std::clamp(v, min, max);
    if (step > 0) v = min + std::round((v - min) / step) * step;
    return std::clamp(std::round(v * 1000.0) / 1000.0, min, max);
  };
  if (!painting()) {
    if (!enabled || !Hit(id)) return false;
    double next = *value;
    if (interaction_->type == Interaction::Type::kSetValue) next = snap(interaction_->value);
    if (interaction_->type == Interaction::Type::kStep) next = snap(*value + interaction_->step * step);
    if (next != *value) {
      *value = next;
      return true;
    }
    return false;
  }
  const float radius = theme::kSliderThumbRadius;
  const D2D1_RECT_F track_area =
      D2D1::RectF(rect.left + radius, rect.top, rect.right - value_width - radius, rect.bottom);
  if (ControlRecord* record = Record(id, ControlKind::kSlider, track_area, name, enabled)) {
    record->slider_min = min;
    record->slider_max = max;
    record->slider_step = step;
    record->slider_value = *value;
    record->value = std::wstring(value_text);
  }
  if (Culled(Inflate(rect, 12.0f))) return false;
  const float center_y = (rect.top + rect.bottom) / 2.0f;
  const bool dragging = enabled && Pressed(id);
  const float fraction = AnimateKeyed(L"slider:", id, static_cast<float>((*value - min) / (max - min)),
                                      dragging ? 0.0f : kThumbMs);
  const float thumb_x = track_area.left + fraction * (track_area.right - track_area.left);
  const bool hover = enabled && (Hovered(id) || Pressed(id));
  const float grown = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  const float held = AnimateKeyed(L"press:", id, dragging ? 1.0f : 0.0f, kPressMs);
  const D2D1_RECT_F track =
      D2D1::RectF(track_area.left - radius, center_y - 2.0f, track_area.right + radius, center_y + 2.0f);
  Fill(Inflate(track, -1.0f), hover ? theme::kSliderTrackHover : theme::kSliderTrack, 1.0f);
  Stroke(track, theme::kSliderTrackBorder, 2.0f);
  Fill(D2D1::RectF(track.left + 2.0f, track.top, track.right - 2.0f, track.top + 1.0f), theme::kSliderTrackEdge);
  const float shown_radius = radius + 1.0f * grown - 2.0f * held;
  const D2D1_RECT_F thumb = D2D1::RectF(thumb_x - shown_radius, center_y - shown_radius, thumb_x + shown_radius,
                                        center_y + shown_radius);
  const D2D1_COLOR_F thumb_fill = enabled ? theme::Rgb(state_->control_accent_rgb) : theme::kTextDisabled;
  Fill(thumb, thumb_fill, shown_radius);
  if (enabled && RelativeLuminance(thumb_fill) + 0.05f < 3.0f * (RelativeLuminance(theme::kField) + 0.05f)) {
    Stroke(thumb, theme::White(0.62f), shown_radius, 1.0f);
  }
  if (value_width > 0.0f) {
    TextLine(D2D1::RectF(rect.right - value_width, rect.top, rect.right, rect.bottom), value_text, TextStyle::kBody,
             enabled ? theme::kText : theme::kTextDisabled, Align::kTrailing);
  }
  FocusRing(id, thumb, radius);
  return false;
}

bool Ui::CheckBox(std::wstring_view id, const D2D1_RECT_F& rect, bool* checked, std::wstring_view name, bool enabled) {
  if (!painting()) {
    if (enabled && Hit(id) && interaction_->type == Interaction::Type::kActivate) {
      *checked = !*checked;
      return true;
    }
    return false;
  }
  if (ControlRecord* record = Record(id, ControlKind::kToggle, rect, name, enabled)) {
    record->checked = *checked;
    record->value = *checked ? T(L"Checked") : T(L"Not checked");
  }
  const float on = Animate(Key(L"check:", id), *checked ? 1.0f : 0.0f, kToggleMs);
  const float h = Animate(Key(L"hover:", id), enabled && Hovered(id) ? 1.0f : 0.0f, kHoverMs);
  const float center_y = (rect.top + rect.bottom) / 2.0f;
  const D2D1_RECT_F box = D2D1::RectF(rect.left, center_y - 10.0f, rect.left + 20.0f, center_y + 10.0f);
  const D2D1_COLOR_F checked_fill =
      enabled ? HoveredControlAccent(state_->control_accent_rgb, h) : theme::kSelectionDisabled;
  const D2D1_COLOR_F empty = theme::White(0.04f + 0.04f * h);
  Fill(box, Mix(empty, checked_fill, on), 4.0f);
  if (on < 1.0f) {
    const D2D1_COLOR_F outline = enabled ? theme::White(0.62f) : theme::White(0.25f);
    Stroke(box, WithAlpha(outline, outline.a * (1.0f - on)), 4.0f, 1.0f);
  }
  if (on > 0.0f) {
    Glyph((box.left + box.right) / 2.0f, center_y, glyph::kCheck, TextStyle::kCaption,
          WithAlpha(enabled ? ContrastingMarker(checked_fill) : theme::White(0.6f), on));
  }
  FocusRing(id, box, 4.0f);
  return false;
}

bool Ui::TextField(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring* text, std::wstring_view placeholder,
                   std::wstring_view name, bool enabled) {
  if (!painting()) {
    if (enabled && Hit(id) && interaction_->type == Interaction::Type::kCommitText) {
      if (*text == interaction_->text) return false;
      *text = interaction_->text;
      return true;
    }
    return false;
  }
  if (ControlRecord* record = Record(id, ControlKind::kTextField, rect, name, enabled)) {
    record->value = *text;
    record->description = std::wstring(placeholder);
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool editing = state_->editing == id;
  const bool hover = enabled && Hovered(id);
  const float h = AnimateKeyed(L"hover:", id, hover && !editing ? 1.0f : 0.0f, kHoverMs);
  const float typing = AnimateKeyed(L"edit:", id, editing ? 1.0f : 0.0f, kSelectMs);
  Fill(rect, Mix(theme::kField, theme::Rgb(theme::kFieldRgb + 0x060606), h), 4.0f);
  Stroke(rect, theme::kFieldBorder, 4.0f);
  const auto focus_line = [&] {
    const float center = (rect.left + rect.right) / 2.0f;
    const float half = ((rect.right - rect.left) / 2.0f - 1.0f) * typing;
    if (half >= 1.0f) {
      Fill(D2D1::RectF(center - half, rect.bottom - 2.0f, center + half, rect.bottom), theme::kFocusRing, 1.0f);
    }
  };
  if (editing) {
    if (typing < 1.0f) {
      Fill(D2D1::RectF(rect.left + 2.0f, rect.bottom - 1.0f, rect.right - 2.0f, rect.bottom), theme::kFieldUnderline,
           0.0f);
    }
    focus_line();
  } else {
    Fill(D2D1::RectF(rect.left + 2.0f, rect.bottom - 1.0f, rect.right - 2.0f, rect.bottom), theme::kFieldUnderline, 0.0f);
    if (typing > 0.0f) focus_line();
    const D2D1_RECT_F inner = D2D1::RectF(rect.left + 11.0f, rect.top, rect.right - 10.0f, rect.bottom);
    if (text->empty()) {
      TextLine(inner, placeholder, TextStyle::kBody, theme::kTextTertiary);
    } else {
      TextLine(inner, *text, TextStyle::kBody, enabled ? theme::kText : theme::kTextDisabled);
    }
  }
  FocusRing(id, rect, 4.0f);
  return false;
}

bool Ui::ListItem(std::wstring_view id, const D2D1_RECT_F& rect, bool selected, std::wstring_view title,
                  std::wstring_view subtitle, std::wstring_view trailing, wchar_t glyph_code, int group,
                  int bitmap_resource) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kListItem, rect, title, true)) {
    record->checked = selected;
    record->value = std::wstring(subtitle);
    record->group = group;
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = Hovered(id);
  const float sel = AnimateKeyed(L"select:", id, selected ? 1.0f : 0.0f, kSelectMs);
  const float h = AnimateKeyed(L"hover:", id, hover && !selected ? 1.0f : 0.0f, kHoverMs);
  const D2D1_COLOR_F fill = Mix(Mix(WithAlpha(theme::kNavHover, 0.0f), theme::kNavHover, h), theme::kNavSelected, sel);
  if (fill.a > 0.0f) Fill(rect, fill, theme::kControlRadius);
  float x = rect.left + 14.0f;
  if (bitmap_resource != 0) {
    constexpr float kArtworkRadius = 16.0f;
    const float center_x = x + 9.0f;
    const float center_y = (rect.top + rect.bottom) / 2.0f;
    Bitmap(D2D1::RectF(center_x - kArtworkRadius, center_y - kArtworkRadius,
                       center_x + kArtworkRadius, center_y + kArtworkRadius), bitmap_resource);
    x += 32.0f;
  } else if (glyph_code != 0) {
    Glyph(x + 9.0f, (rect.top + rect.bottom) / 2.0f, glyph_code, TextStyle::kIcon,
          selected ? theme::kText : theme::kTextSecondary);
    x += 32.0f;
  }
  const float trailing_width = trailing.empty() ? 0.0f : TextWidth(trailing, TextStyle::kCaption) + 8.0f;
  const float mid = (rect.top + rect.bottom) / 2.0f;
  TextLine(D2D1::RectF(x, rect.top + 6.0f, rect.right - 12.0f - trailing_width, mid + 1.0f), title, TextStyle::kBody,
           theme::kText);
  TextLine(D2D1::RectF(x, mid - 1.0f, rect.right - 12.0f, rect.bottom - 6.0f), subtitle, TextStyle::kCaption,
           theme::kTextTertiary);
  if (!trailing.empty()) {
    TextLine(D2D1::RectF(rect.right - 12.0f - trailing_width, rect.top + 6.0f, rect.right - 12.0f, mid + 1.0f),
             trailing, TextStyle::kCaption, theme::kTextSecondary, Align::kTrailing);
  }
  FocusRing(id, rect, theme::kControlRadius);
  return false;
}

bool Ui::Radio(std::wstring_view id, const D2D1_RECT_F& rect, bool selected, std::wstring_view label,
               std::wstring_view detail, int group) {
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  if (ControlRecord* record = Record(id, ControlKind::kRadio, rect, label, true)) {
    record->checked = selected;
    record->description = std::wstring(detail);
    record->group = group;
  }
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = Hovered(id);
  const float h = AnimateKeyed(L"hover:", id, hover ? 1.0f : 0.0f, kHoverMs);
  if (h > 0.0f) Fill(rect, WithAlpha(theme::kNavHover, theme::kNavHover.a * h), theme::kControlRadius);
  const float cx = rect.left + 20.0f;
  const float cy = (rect.top + rect.bottom) / 2.0f;
  const float sel = AnimateKeyed(L"radio:", id, selected ? 1.0f : 0.0f, kToggleMs);
  const D2D1_RECT_F ring = D2D1::RectF(cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f);
  if (sel < 1.0f) Stroke(ring, theme::White((0.6f + 0.2f * h) * (1.0f - sel)), 9.0f, 1.0f);
  if (sel > 0.0f) {
    const D2D1_COLOR_F accent = theme::Rgb(state_->control_accent_rgb);
    Fill(ring, WithAlpha(accent, sel), 9.0f);
    const float dot = 4.0f * sel;
    Fill(D2D1::RectF(cx - dot, cy - dot, cx + dot, cy + dot), WithAlpha(ContrastingMarker(accent), sel), dot);
  }
  const float label_width = std::min(TextWidth(label, TextStyle::kBody) + 2.0f, rect.right - cx - 24.0f);
  TextLine(D2D1::RectF(cx + 20.0f, rect.top, cx + 20.0f + label_width, rect.bottom), label, TextStyle::kBody,
           theme::kText);
  if (!detail.empty()) {
    TextLine(D2D1::RectF(cx + 32.0f + label_width, rect.top, rect.right - 10.0f, rect.bottom), detail,
             TextStyle::kCaption, theme::kTextTertiary);
  }
  FocusRing(id, rect, theme::kControlRadius);
  return false;
}

bool Ui::Link(std::wstring_view id, float x, float y, std::wstring_view text, float* out_width, TextStyle style) {
  const float width = TextWidth(text, style) + 1.0f;
  const float height = renderer_->Measure(L"Ag", style, 0.0f, false).height;
  if (out_width != nullptr) *out_width = width;
  if (!painting()) return Hit(id) && interaction_->type == Interaction::Type::kActivate;
  const D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + height);
  Record(id, ControlKind::kLink, rect, text, true);
  if (Culled(Inflate(rect, 4.0f))) return false;
  const bool hover = Hovered(id);
  TextLine(rect, text, style, hover ? theme::kFocusRing : theme::kText);
  Fill(D2D1::RectF(x, rect.bottom - 1.0f, x + width - 1.0f, rect.bottom),
       hover ? theme::kFocusRing : theme::White(0.45f));
  FocusRing(id, rect, 3.0f);
  return false;
}

bool Ui::TextView(std::wstring_view id, const D2D1_RECT_F& rect, std::wstring_view text, float* scroll,
                  std::wstring_view name) {
  const float pad_x = 16.0f;
  const float pad_y = 12.0f;
  const float text_width = rect.right - rect.left - 2.0f * pad_x - 8.0f;
  const float view_height = rect.bottom - rect.top - 2.0f * pad_y;
  const float line_height = std::max(1.0f, renderer_->Measure(L"Ag", TextStyle::kMono, 0.0f, false).height);
  const float char_width = renderer_->Measure(L"0000000000", TextStyle::kMono, 0.0f, false).width / 10.0f;
  const int columns = char_width > 0.0f && text_width > 0.0f
                          ? std::max(1, static_cast<int>(std::floor((text_width - 1.0f) / char_width)))
                          : 1;
  const Renderer::WrappedText& wrapped = renderer_->Wrap(text, columns);
  const float content =
      text.empty() || text_width <= 0 ? 0.0f : static_cast<float>(wrapped.rows.size()) * line_height;
  const float max_scroll = std::max(0.0f, content - view_height);
  if (!painting()) {
    if (!Hit(id)) return false;
    float next = *scroll;
    if (interaction_->type == Interaction::Type::kStep) next += static_cast<float>(interaction_->step) * line_height;
    if (interaction_->type == Interaction::Type::kSetValue) next = static_cast<float>(interaction_->value);
    next = std::clamp(next, 0.0f, max_scroll);
    if (next == *scroll) return false;
    *scroll = next;
    return true;
  }
  *scroll = std::clamp(*scroll, 0.0f, max_scroll);
  if (ControlRecord* record = Record(id, ControlKind::kTextView, rect, name, true)) {
    record->value = std::wstring(text.substr(0, std::min<std::size_t>(text.size(), 4000)));
    record->slider_max = max_scroll;
    record->slider_value = *scroll;
  }
  if (Culled(rect)) return false;
  PushClip(D2D1::RectF(rect.left, rect.top + 1.0f, rect.right, rect.bottom - 1.0f));
  if (content > 0.0f) {
    const std::size_t first = static_cast<std::size_t>(std::max(0.0f, std::floor((*scroll - pad_y) / line_height)));
    const std::size_t last = std::min(
        wrapped.rows.size(), static_cast<std::size_t>(std::ceil((*scroll + view_height + pad_y) / line_height)) + 1);
    std::wstring visible;
    for (std::size_t row = first; row < last; ++row) {
      if (row > first) visible.push_back(L'\n');
      visible.append(wrapped.display, wrapped.rows[row].first, wrapped.rows[row].second);
    }
    if (!visible.empty()) {
      ComPtr<IDWriteTextLayout> layout = renderer_->UncachedLayout(visible, TextStyle::kMono, text_width + char_width);
      if (layout) {
        float baseline = line_height * 0.8f;
        if (IDWriteTextLayout* sample = renderer_->Layout(L"Ag", TextStyle::kMono, 0.0f, false)) {
          DWRITE_LINE_METRICS line{};
          UINT32 count = 0;
          if (SUCCEEDED(sample->GetLineMetrics(&line, 1, &count)) && count == 1) baseline = line.baseline;
        }
        layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line_height, baseline);
        const float y = rect.top + pad_y - *scroll + static_cast<float>(first) * line_height;
        DWRITE_TEXT_METRICS metrics{};
        layout->GetMetrics(&metrics);
        commands_.push_back(Command{Command::Type::kText,
                                    D2D1::RectF(rect.left + pad_x, y, rect.left + pad_x + text_width, y + metrics.height),
                                    theme::kLogText, {}, 0, 0, layout});
        LogText(visible, D2D1::RectF(rect.left + pad_x, y, rect.left + pad_x + std::min(metrics.width, text_width),
                                     y + metrics.height));
      }
    }
  }
  PopClip();
  if (max_scroll > 0.0f) {
    const float track = rect.bottom - rect.top - 16.0f;
    const float thumb = std::max(28.0f, track * view_height / content);
    const float offset = (track - thumb) * (*scroll / max_scroll);
    Fill(D2D1::RectF(rect.right - 8.0f, rect.top + 8.0f + offset, rect.right - 4.0f, rect.top + 8.0f + offset + thumb),
         theme::White(Hovered(id) ? 0.34f : 0.20f), 2.0f);
  }
  FocusRing(id, D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, rect.bottom - 2.0f), 6.0f);
  return false;
}

void Ui::Flush() {
  ID2D1RenderTarget* target = renderer_->target();
  if (target == nullptr) return;
  const float scale = renderer_->dpi() / 96.0f;
  const auto snap = [scale](float v) { return std::round(v * scale) / scale; };
  struct OpenLayer {
    bool pushed;
    D2D1_MATRIX_3X2_F transform;
  };
  std::vector<OpenLayer> layers;
  for (const Command& command : commands_) {
    switch (command.type) {
      case Command::Type::kFill:
        if (command.radius > 0) {
          target->FillRoundedRectangle(D2D1::RoundedRect(command.rect, command.radius, command.radius),
                                       renderer_->Brush(command.color));
        } else {
          target->FillRectangle(command.rect, renderer_->Brush(command.color));
        }
        break;
      case Command::Type::kStroke:
        if (command.radius > 0) {
          target->DrawRoundedRectangle(D2D1::RoundedRect(command.rect, command.radius, command.radius),
                                       renderer_->Brush(command.color), command.width);
        } else {
          target->DrawRectangle(command.rect, renderer_->Brush(command.color), command.width);
        }
        break;
      case Command::Type::kLine:
        target->DrawLine(D2D1::Point2F(command.rect.left, command.rect.top),
                         D2D1::Point2F(command.rect.right, command.rect.bottom), renderer_->Brush(command.color),
                         command.width);
        break;
      case Command::Type::kGradient: {
        if (ID2D1LinearGradientBrush* brush = renderer_->Gradient(command.color, command.color2)) {
          brush->SetStartPoint(D2D1::Point2F(command.rect.left, command.rect.top));
          brush->SetEndPoint(D2D1::Point2F(command.rect.right, command.rect.bottom));
          target->FillRoundedRectangle(D2D1::RoundedRect(command.rect, command.radius, command.radius), brush);
        }
        break;
      }
      case Command::Type::kNone:
        break;
      case Command::Type::kText:
        target->DrawTextLayout(D2D1::Point2F(snap(command.rect.left), snap(command.rect.top)), command.layout.Get(),
                               renderer_->Brush(command.color), D2D1_DRAW_TEXT_OPTIONS_NONE);
        break;
      case Command::Type::kBitmap: {
        const float left = snap(command.rect.left);
        const float top = snap(command.rect.top);
        const UINT width_px = static_cast<UINT>(std::lround((command.rect.right - command.rect.left) * scale));
        const UINT height_px = static_cast<UINT>(std::lround((command.rect.bottom - command.rect.top) * scale));
        if (ID2D1Bitmap* bitmap = command.bitmap_file.empty()
                                      ? renderer_->ResourceBitmap(command.bitmap_resource, width_px, height_px)
                                      : renderer_->FileBitmap(command.bitmap_file, width_px, height_px)) {
          target->DrawBitmap(bitmap,
                             D2D1::RectF(left, top, left + width_px / scale, top + height_px / scale),
                             std::clamp(command.color.a, 0.0f, 1.0f), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        break;
      }
      case Command::Type::kIcon: {
        ID2D1Geometry* geometry = renderer_->Icon(command.icon);
        if (geometry == nullptr) break;
        const float icon_scale = (command.rect.right - command.rect.left) / 24.0f;
        D2D1_MATRIX_3X2_F saved{};
        target->GetTransform(&saved);
        target->SetTransform(D2D1::Matrix3x2F::Scale(icon_scale, icon_scale) *
                             D2D1::Matrix3x2F::Translation(snap(command.rect.left), snap(command.rect.top)) * saved);
        target->DrawGeometry(geometry, renderer_->Brush(command.color), std::max(2.0f, 1.25f / icon_scale),
                             renderer_->IconStroke());
        target->SetTransform(saved);
        break;
      }
      case Command::Type::kPushClip:
        target->PushAxisAlignedClip(command.rect, D2D1_ANTIALIAS_MODE_ALIASED);
        break;
      case Command::Type::kPopClip:
        target->PopAxisAlignedClip();
        break;
      case Command::Type::kPushLayer: {
        OpenLayer layer{false, {}};
        target->GetTransform(&layer.transform);
        const float opacity = command.color.a;
        const float dy = snap(command.radius);
        if (opacity < 0.999f) {
          target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                  D2D1::IdentityMatrix(), opacity),
                            nullptr);
          layer.pushed = true;
        }
        if (dy != 0.0f) target->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, dy) * layer.transform);
        layers.push_back(layer);
        break;
      }
      case Command::Type::kPopLayer:
        if (!layers.empty()) {
          if (layers.back().pushed) target->PopLayer();
          target->SetTransform(layers.back().transform);
          layers.pop_back();
        }
        break;
    }
  }
  while (!layers.empty()) {
    if (layers.back().pushed) target->PopLayer();
    target->SetTransform(layers.back().transform);
    layers.pop_back();
  }
  commands_.clear();
}

}
