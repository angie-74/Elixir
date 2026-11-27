// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "ui/renderer.h"

#include "ui/lucide.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <functional>

#include "core/i18n.h"
#include "resource.h"

namespace elixir::launcher::ui {

namespace {

struct FormatSpec {
  int family;
  float size;
  DWRITE_FONT_WEIGHT weight;
};

constexpr FormatSpec kSpecs[] = {
    {1, 28.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {1, 20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {0, 15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {0, 14.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {0, 13.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 12.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {3, 12.5f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 13.5f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 14.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {1, 20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {0, 18.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {0, 10.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {2, 16.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {2, 12.0f, DWRITE_FONT_WEIGHT_NORMAL},
    {2, 20.0f, DWRITE_FONT_WEIGHT_NORMAL},
};
static_assert(sizeof(kSpecs) / sizeof(kSpecs[0]) == static_cast<int>(TextStyle::kCount));

ComPtr<IWICBitmapSource> LoadPngResource(IWICImagingFactory* wic, int id) {
  if (wic == nullptr) return nullptr;
  HMODULE module = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10) );
  if (resource == nullptr) return nullptr;
  HGLOBAL loaded = LoadResource(module, resource);
  const DWORD size = SizeofResource(module, resource);
  void* data = loaded != nullptr ? LockResource(loaded) : nullptr;
  if (data == nullptr || size == 0) return nullptr;
  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  ComPtr<IWICBitmap> bitmap;
  if (FAILED(wic->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(data), size)) ||
      FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
      FAILED(decoder->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&converter)) ||
      FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom)) ||
      FAILED(wic->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &bitmap))) {
    return nullptr;
  }
  return bitmap;
}

}

Renderer::~Renderer() = default;

std::wstring Renderer::PickFamily(std::initializer_list<const wchar_t*> candidates) {
  ComPtr<IDWriteFontCollection> fonts;
  if (SUCCEEDED(dwrite_->GetSystemFontCollection(&fonts, FALSE))) {
    for (const wchar_t* candidate : candidates) {
      UINT32 index = 0;
      BOOL exists = FALSE;
      if (SUCCEEDED(fonts->FindFamilyName(candidate, &index, &exists)) && exists) return candidate;
    }
  }
  return *(candidates.end() - 1);
}

bool Renderer::Initialize() {
  D2D1_FACTORY_OPTIONS options{};
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &options,
                               reinterpret_cast<void**>(factory_.GetAddressOf())))) {
    return false;
  }
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf())))) {
    return false;
  }
  CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_));
  text_family_ = PickFamily({L"Segoe UI Variable Text", L"Segoe UI"});
  display_family_ = PickFamily({L"Segoe UI Variable Display", L"Segoe UI"});
  icon_family_ = PickFamily({L"Segoe Fluent Icons", L"Segoe MDL2 Assets"});
  mono_family_ = PickFamily({L"Cascadia Mono", L"Consolas"});
  locale_ = CurrentLanguageTag();
  right_to_left_ = CurrentLanguageIsRightToLeft();
  language_generation_ = LanguageGeneration();
  if (!CreateFormats()) return false;
  bitmap_sources_[IDR_ELIXIR_LOGO_PNG] = LoadPngResource(wic_.Get(), IDR_ELIXIR_LOGO_PNG);
  return true;
}

bool Renderer::CreateFormats() {
  const std::wstring* families[] = {&text_family_, &display_family_, &icon_family_, &mono_family_};
  for (int i = 0; i < static_cast<int>(TextStyle::kCount); ++i) {
    const FormatSpec& spec = kSpecs[i];
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(dwrite_->CreateTextFormat(families[spec.family]->c_str(), nullptr, spec.weight,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, spec.size,
                                         locale_.c_str(), &format))) {
      return false;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    formats_[i] = format;
    ellipsis_sign_[i].Reset();
    dwrite_->CreateEllipsisTrimmingSign(formats_[i].Get(), &ellipsis_sign_[i]);
  }
  layouts_.clear();
  return true;
}

bool Renderer::SyncLanguage() {
  const std::uint64_t generation = LanguageGeneration();
  if (generation == language_generation_ || !dwrite_) return false;
  language_generation_ = generation;
  const std::wstring locale = CurrentLanguageTag();
  const bool right_to_left = CurrentLanguageIsRightToLeft();
  if (locale == locale_ && right_to_left == right_to_left_) return false;
  const std::wstring previous = locale_;
  locale_ = locale;
  right_to_left_ = right_to_left;
  if (!CreateFormats()) {
    locale_ = previous;
    CreateFormats();
  }
  return true;
}

void Renderer::ApplyDirection(IDWriteTextLayout* layout, std::wstring_view text, Align align) const {
  DWRITE_TEXT_ALIGNMENT alignment = align == Align::kCenter     ? DWRITE_TEXT_ALIGNMENT_CENTER
                                    : align == Align::kTrailing ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                                                : DWRITE_TEXT_ALIGNMENT_LEADING;
  if (right_to_left_ && ReadsRightToLeft(text)) {
    layout->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    if (alignment == DWRITE_TEXT_ALIGNMENT_LEADING) {
      alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
    } else if (alignment == DWRITE_TEXT_ALIGNMENT_TRAILING) {
      alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    }
  }
  layout->SetTextAlignment(alignment);
}

bool Renderer::EnsureTarget(HWND hwnd, UINT dpi) {
  SyncLanguage();
  if (target_) return true;
  RECT client{};
  GetClientRect(hwnd, &client);
  dpi_ = static_cast<float>(dpi);
  const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
      dpi_, dpi_);
  const D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_properties = D2D1::HwndRenderTargetProperties(
      hwnd,
      D2D1::SizeU(static_cast<UINT32>(client.right > 0 ? client.right : 1),
                  static_cast<UINT32>(client.bottom > 0 ? client.bottom : 1)),
      D2D1_PRESENT_OPTIONS_IMMEDIATELY);
  if (FAILED(factory_->CreateHwndRenderTarget(properties, hwnd_properties, &target_))) return false;
  target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  brush_.Reset();
  gradients_.clear();
  bitmaps_.clear();
  files_.clear();
  return true;
}

void Renderer::Resize(UINT width, UINT height) {
  if (target_) target_->Resize(D2D1::SizeU(width > 0 ? width : 1, height > 0 ? height : 1));
}

void Renderer::SetDpi(UINT dpi) {
  if (offscreen_) {
    window_dpi_ = static_cast<float>(dpi);
  } else {
    dpi_ = static_cast<float>(dpi);
    bitmaps_.clear();
    files_.clear();
  }
  if (target_) target_->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
}

void Renderer::DiscardTarget() {
  target_.Reset();
  brush_.Reset();
  gradients_.clear();
  bitmaps_.clear();
  files_.clear();
}

void Renderer::BeginOffscreen(ID2D1RenderTarget* target, float dpi) {
  SyncLanguage();
  if (!offscreen_) window_dpi_ = dpi_;
  offscreen_ = target;
  dpi_ = dpi;
  brush_.Reset();
  gradients_.clear();
  bitmaps_.clear();
  files_.clear();
}

void Renderer::EndOffscreen() {
  if (!offscreen_) return;
  offscreen_.Reset();
  dpi_ = window_dpi_;
  brush_.Reset();
  gradients_.clear();
  bitmaps_.clear();
  files_.clear();
}

ID2D1LinearGradientBrush* Renderer::Gradient(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to) {
  ID2D1RenderTarget* active = target();
  if (active == nullptr) return nullptr;
  for (const GradientEntry& entry : gradients_) {
    if (std::memcmp(&entry.from, &from, sizeof(from)) == 0 && std::memcmp(&entry.to, &to, sizeof(to)) == 0) {
      return entry.brush.Get();
    }
  }
  const D2D1_GRADIENT_STOP stops[2] = {{0.0f, from}, {1.0f, to}};
  ComPtr<ID2D1GradientStopCollection> collection;
  ComPtr<ID2D1LinearGradientBrush> brush;
  if (FAILED(active->CreateGradientStopCollection(stops, 2, &collection)) ||
      FAILED(active->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(D2D1::Point2F(), D2D1::Point2F(0.0f, 1.0f)), collection.Get(),
          &brush))) {
    return nullptr;
  }
  if (gradients_.size() >= 16) gradients_.clear();
  gradients_.push_back({from, to, brush});
  return brush.Get();
}

ID2D1SolidColorBrush* Renderer::Brush(const D2D1_COLOR_F& color) {
  ID2D1RenderTarget* active = target();
  if (!brush_ && active != nullptr) active->CreateSolidColorBrush(color, &brush_);
  if (brush_) brush_->SetColor(color);
  return brush_.Get();
}

IDWriteTextLayout* Renderer::Layout(std::wstring_view text, TextStyle style, float max_width, bool wrap, Align align,
                                    bool ellipsis) {
  const std::int32_t half_dips = static_cast<std::int32_t>(std::lround(max_width * 2.0f));
  const std::uint32_t params = static_cast<std::uint32_t>(style) | (wrap ? 0x100u : 0u) |
                               (static_cast<std::uint32_t>(align) << 9) | (ellipsis ? 0x1000u : 0u);
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
  };
  mix(params);
  mix(static_cast<std::uint32_t>(half_dips));
  for (const wchar_t c : text) mix(static_cast<std::uint16_t>(c));
  auto found = layouts_.find(hash);
  if (found != layouts_.end() && found->second.params == params && found->second.width == half_dips &&
      found->second.text == text) {
    return found->second.layout.Get();
  }
  if (layouts_.size() > 3000) layouts_.clear();

  ComPtr<IDWriteTextLayout> layout;
  const float width = max_width > 0 ? max_width : 100000.0f;
  if (FAILED(dwrite_->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), Format(style), width, 100000.0f,
                                       &layout))) {
    return nullptr;
  }
  layout->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
  ApplyDirection(layout.Get(), text, align);
  if (ellipsis) {
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    layout->SetTrimming(&trimming, ellipsis_sign_[static_cast<int>(style)].Get());
  }
  LayoutEntry& entry = layouts_[hash];
  entry.text.assign(text);
  entry.params = params;
  entry.width = half_dips;
  entry.layout = std::move(layout);
  return entry.layout.Get();
}

IDWriteTextLayout* Renderer::FittedLayout(std::wstring_view text, TextStyle style, float max_width, Align align,
                                          float min_scale) {
  if (max_width <= 0.0f || min_scale >= 1.0f || text.empty()) return Layout(text, style, max_width, false, align, true);
  const float natural = Measure(text, style, 0.0f, false).width;
  if (natural <= max_width) return Layout(text, style, max_width, false, align, true);
  const float scale = std::max(min_scale, max_width / natural * 0.995f);
  const std::int32_t half_dips = static_cast<std::int32_t>(std::lround(max_width * 2.0f));
  const std::uint32_t params = static_cast<std::uint32_t>(style) | (static_cast<std::uint32_t>(align) << 9) | 0x3000u |
                               (static_cast<std::uint32_t>(std::lround(min_scale * 1000.0f)) << 14);
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ull;
  };
  mix(params);
  mix(static_cast<std::uint32_t>(half_dips));
  for (const wchar_t c : text) mix(static_cast<std::uint16_t>(c));
  auto found = layouts_.find(hash);
  if (found != layouts_.end() && found->second.params == params && found->second.width == half_dips &&
      found->second.text == text) {
    return found->second.layout.Get();
  }
  if (layouts_.size() > 3000) layouts_.clear();
  ComPtr<IDWriteTextLayout> layout;
  if (FAILED(dwrite_->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), Format(style), max_width,
                                       100000.0f, &layout))) {
    return nullptr;
  }
  layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  ApplyDirection(layout.Get(), text, align);
  layout->SetFontSize(kSpecs[static_cast<int>(style)].size * scale,
                      DWRITE_TEXT_RANGE{0, static_cast<UINT32>(text.size())});
  DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
  layout->SetTrimming(&trimming, ellipsis_sign_[static_cast<int>(style)].Get());
  LayoutEntry& entry = layouts_[hash];
  entry.text.assign(text);
  entry.params = params;
  entry.width = half_dips;
  entry.layout = std::move(layout);
  return entry.layout.Get();
}

ComPtr<IDWriteTextLayout> Renderer::UncachedLayout(std::wstring_view text, TextStyle style, float max_width) {
  ComPtr<IDWriteTextLayout> layout;
  if (FAILED(dwrite_->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), Format(style),
                                       max_width > 0 ? max_width : 100000.0f, 100000.0f, &layout))) {
    return nullptr;
  }
  layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  return layout;
}

const Renderer::WrappedText& Renderer::Wrap(std::wstring_view text, int columns) {
  columns = columns < 1 ? 1 : columns;
  const std::size_t hash = std::hash<std::wstring_view>{}(text);
  if (wrapped_.columns == columns && wrapped_.source_size == text.size() && wrapped_.source_hash == hash) {
    return wrapped_;
  }
  WrappedText wrapped;
  wrapped.source_hash = hash;
  wrapped.source_size = text.size();
  wrapped.columns = columns;
  wrapped.display.reserve(text.size());
  std::size_t line_start = 0;
  const auto close_line = [&] {
    const std::size_t end = wrapped.display.size();
    std::size_t start = line_start;
    do {
      std::size_t length = std::min<std::size_t>(static_cast<std::size_t>(columns), end - start);
      std::size_t next = start + length;
      if (next < end) {
        for (std::size_t k = length; k > length / 2; --k) {
          if (wrapped.display[start + k] == L' ') {
            length = k;
            next = start + k + 1;
            break;
          }
        }
        if (next == start + length && length > 1 && IS_HIGH_SURROGATE(wrapped.display[start + length - 1])) {
          --length;
          --next;
        }
      }
      wrapped.rows.emplace_back(static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(length));
      start = next;
    } while (start < end);
  };
  for (const wchar_t c : text) {
    if (c == L'\r') continue;
    if (c == L'\n') {
      close_line();
      wrapped.display.push_back(L'\n');
      line_start = wrapped.display.size();
      continue;
    }
    if (c == L'\t') {
      const std::size_t column = wrapped.display.size() - line_start;
      wrapped.display.append(4 - column % 4, L' ');
      continue;
    }
    wrapped.display.push_back(c);
  }
  if (line_start < wrapped.display.size() || wrapped.rows.empty()) close_line();
  wrapped_ = std::move(wrapped);
  return wrapped_;
}

D2D1_SIZE_F Renderer::Measure(std::wstring_view text, TextStyle style, float max_width, bool wrap) {
  IDWriteTextLayout* layout = Layout(text, style, max_width, wrap);
  DWRITE_TEXT_METRICS metrics{};
  if (layout == nullptr || FAILED(layout->GetMetrics(&metrics))) return D2D1::SizeF(0, 0);
  return D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height);
}

ID2D1Geometry* Renderer::Icon(wchar_t glyph) {
  if (glyph == 0 || !factory_) return nullptr;
  const auto found = icons_.find(glyph);
  if (found != icons_.end()) return found->second.Get();
  ComPtr<ID2D1PathGeometry> geometry;
  const lucide::Icon icon = lucide::IconFor(glyph);
  if (!icon.svg.empty() && !lucide::BuildGeometry(factory_.Get(), icon.svg, &geometry)) geometry.Reset();
  return icons_.emplace(glyph, std::move(geometry)).first->second.Get();
}

ID2D1StrokeStyle* Renderer::IconStroke() {
  if (!icon_stroke_ && factory_) {
    const D2D1_STROKE_STYLE_PROPERTIES properties = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f);
    factory_->CreateStrokeStyle(properties, nullptr, 0, &icon_stroke_);
  }
  return icon_stroke_.Get();
}

ID2D1Bitmap* Renderer::ResourceBitmap(int resource_id, UINT width_px, UINT height_px) {
  ID2D1RenderTarget* active = target();
  if (active == nullptr || width_px == 0 || height_px == 0) return nullptr;
  for (const ScaledBitmap& bitmap : bitmaps_) {
    if (bitmap.resource_id == resource_id && bitmap.width == width_px && bitmap.height == height_px) {
      return bitmap.bitmap.Get();
    }
  }
  auto [source, newly_loaded] = bitmap_sources_.try_emplace(resource_id);
  if (newly_loaded) source->second = LoadPngResource(wic_.Get(), resource_id);
  if (!source->second) return nullptr;
  ScaledBitmap bitmap{resource_id, width_px, height_px, nullptr};
  ComPtr<IWICBitmapScaler> scaler;
  if (FAILED(wic_->CreateBitmapScaler(&scaler)) ||
      FAILED(scaler->Initialize(source->second.Get(), width_px, height_px, WICBitmapInterpolationModeHighQualityCubic)) ||
      FAILED(active->CreateBitmapFromWicBitmap(scaler.Get(), nullptr, &bitmap.bitmap))) {
    return nullptr;
  }
  if (bitmaps_.size() >= kMaxScaledBitmaps) bitmaps_.erase(bitmaps_.begin());
  bitmaps_.push_back(std::move(bitmap));
  return bitmaps_.back().bitmap.Get();
}

ID2D1Bitmap* Renderer::FileBitmap(const std::wstring& path, UINT width_px, UINT height_px) {
  ID2D1RenderTarget* active = target();
  if (active == nullptr || wic_ == nullptr || path.empty() || width_px == 0 || height_px == 0) return nullptr;
  for (const ScaledFile& file : files_) {
    if (file.width == width_px && file.height == height_px && file.path == path) return file.bitmap.Get();
  }
  ScaledFile file{path, width_px, height_px, nullptr};
  ComPtr<IWICBitmapDecoder> decoder;
  if (SUCCEEDED(wic_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                &decoder))) {
    UINT frames = 0;
    decoder->GetFrameCount(&frames);
    ComPtr<IWICBitmapFrameDecode> best;
    UINT best_width = 0;
    for (UINT i = 0; i < frames && i < 32; ++i) {
      ComPtr<IWICBitmapFrameDecode> frame;
      UINT width = 0;
      UINT height = 0;
      if (FAILED(decoder->GetFrame(i, &frame)) || FAILED(frame->GetSize(&width, &height))) continue;
      if (width > best_width) {
        best_width = width;
        best = frame;
      }
    }
    ComPtr<IWICFormatConverter> converter;
    ComPtr<IWICBitmap> pixels;
    ComPtr<IWICBitmapScaler> scaler;
    if (best && SUCCEEDED(wic_->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(best.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr,
                                        0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(wic_->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &pixels)) &&
        SUCCEEDED(wic_->CreateBitmapScaler(&scaler)) &&
        SUCCEEDED(scaler->Initialize(pixels.Get(), width_px, height_px, WICBitmapInterpolationModeHighQualityCubic))) {
      active->CreateBitmapFromWicBitmap(scaler.Get(), nullptr, &file.bitmap);
    }
  }
  if (files_.size() >= kMaxScaledFiles) files_.erase(files_.begin());
  files_.push_back(std::move(file));
  return files_.back().bitmap.Get();
}

}
