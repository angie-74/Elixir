// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_RENDERER_H_
#define ELIXIR_LAUNCHER_UI_RENDERER_H_

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elixir::launcher::ui {

using Microsoft::WRL::ComPtr;

enum class TextStyle {
  kTitle,
  kSection,
  kCardTitle,
  kBody,
  kBodyStrong,
  kSecondary,
  kCaption,
  kCaptionStrong,
  kMono,
  kTagline,
  kButton,
  kButtonLarge,
  kHero,
  kStatus,
  kMicro,
  kIcon,
  kIconSmall,
  kIconLarge,
  kCount,
};

enum class Align { kLeading, kCenter, kTrailing };

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool Initialize();

  bool EnsureTarget(HWND hwnd, UINT dpi);
  void Resize(UINT width, UINT height);
  void SetDpi(UINT dpi);
  void DiscardTarget();
  ID2D1RenderTarget* target() const {
    return offscreen_ ? offscreen_.Get() : static_cast<ID2D1RenderTarget*>(target_.Get());
  }
  float dpi() const { return dpi_; }

  void BeginOffscreen(ID2D1RenderTarget* target, float dpi);
  bool SyncLanguage();
  void EndOffscreen();
  ID2D1Factory* factory() const { return factory_.Get(); }
  IWICImagingFactory* wic() const { return wic_.Get(); }

  ID2D1SolidColorBrush* Brush(const D2D1_COLOR_F& color);
  ID2D1LinearGradientBrush* Gradient(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to);

  IDWriteTextLayout* Layout(std::wstring_view text, TextStyle style, float max_width, bool wrap = true,
                            Align align = Align::kLeading, bool ellipsis = false);
  IDWriteTextLayout* FittedLayout(std::wstring_view text, TextStyle style, float max_width, Align align,
                                  float min_scale);
  D2D1_SIZE_F Measure(std::wstring_view text, TextStyle style, float max_width, bool wrap = true);
  ComPtr<IDWriteTextLayout> UncachedLayout(std::wstring_view text, TextStyle style, float max_width);

  struct WrappedText {
    std::size_t source_hash = 0;
    std::size_t source_size = 0;
    int columns = 0;
    std::wstring display;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rows;
  };
  const WrappedText& Wrap(std::wstring_view text, int columns);

  ID2D1Bitmap* ResourceBitmap(int resource_id, UINT width_px, UINT height_px);
  ID2D1Bitmap* FileBitmap(const std::wstring& path, UINT width_px, UINT height_px);
  ID2D1Geometry* Icon(wchar_t glyph);
  ID2D1StrokeStyle* IconStroke();

  const std::wstring& text_family() const { return text_family_; }
  const std::wstring& icon_family() const { return icon_family_; }
  const std::wstring& mono_family() const { return mono_family_; }

 private:
  IDWriteTextFormat* Format(TextStyle style) { return formats_[static_cast<int>(style)].Get(); }
  bool CreateFormats();
  void ApplyDirection(IDWriteTextLayout* layout, std::wstring_view text, Align align) const;
  std::wstring PickFamily(std::initializer_list<const wchar_t*> candidates);

  ComPtr<ID2D1Factory> factory_;
  ComPtr<IDWriteFactory> dwrite_;
  ComPtr<IWICImagingFactory> wic_;
  ComPtr<ID2D1HwndRenderTarget> target_;
  ComPtr<ID2D1RenderTarget> offscreen_;
  float window_dpi_ = 96.0f;
  ComPtr<ID2D1SolidColorBrush> brush_;
  std::unordered_map<int, ComPtr<IWICBitmapSource>> bitmap_sources_;
  struct ScaledBitmap {
    int resource_id = 0;
    UINT width = 0;
    UINT height = 0;
    ComPtr<ID2D1Bitmap> bitmap;
  };
  static constexpr std::size_t kMaxScaledBitmaps = 4;
  std::vector<ScaledBitmap> bitmaps_;
  struct ScaledFile {
    std::wstring path;
    UINT width = 0;
    UINT height = 0;
    ComPtr<ID2D1Bitmap> bitmap;
  };
  static constexpr std::size_t kMaxScaledFiles = 32;
  std::vector<ScaledFile> files_;
  std::unordered_map<wchar_t, ComPtr<ID2D1PathGeometry>> icons_;
  ComPtr<ID2D1StrokeStyle> icon_stroke_;
  std::array<ComPtr<IDWriteTextFormat>, static_cast<int>(TextStyle::kCount)> formats_;
  ComPtr<IDWriteInlineObject> ellipsis_sign_[static_cast<int>(TextStyle::kCount)];
  struct LayoutEntry {
    std::wstring text;
    std::uint32_t params = 0;
    std::int32_t width = 0;
    ComPtr<IDWriteTextLayout> layout;
  };
  std::unordered_map<std::uint64_t, LayoutEntry> layouts_;
  struct GradientEntry {
    D2D1_COLOR_F from;
    D2D1_COLOR_F to;
    ComPtr<ID2D1LinearGradientBrush> brush;
  };
  std::vector<GradientEntry> gradients_;
  WrappedText wrapped_;
  std::wstring text_family_;
  std::wstring display_family_;
  std::wstring icon_family_;
  std::wstring mono_family_;
  std::wstring locale_ = L"en-us";
  bool right_to_left_ = false;
  std::uint64_t language_generation_ = 0;
  float dpi_ = 96.0f;
};

}

#endif
