// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "runtime/roblox_text_surface_overlay.h"

#include <windows.h>

#include <d2d1.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "runtime/roblox_text_font_resolver.h"

namespace mocktail {
namespace runtime {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::size_t kMaximumRasterTextBytes = 4096;
constexpr std::uint64_t kMaximumOverlayPixels = 16ULL * 1024ULL * 1024ULL;
constexpr float kMinimumRasterScale = 0.25F;
constexpr float kMaximumRasterScale = 8.0F;
constexpr float kUnboundedLayoutWidth = 1.0e6F;
constexpr wchar_t kFallbackFamily[] = L"Segoe UI";

std::mutex g_active_overlay_mutex;
RobloxTextSurfaceOverlay* g_active_overlay = nullptr;
std::atomic<bool> g_overlay_may_present{false};

void SecureClear(std::string* value) {
  if (value == nullptr) {
    return;
  }
  volatile char* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t index = 0; index < value->size(); ++index) {
    bytes[index] = 0;
  }
  value->clear();
}

void SecureClear(std::wstring* value) {
  if (value == nullptr) {
    return;
  }
  if (!value->empty()) {
    SecureZeroMemory(value->data(), value->size() * sizeof(wchar_t));
  }
  value->clear();
}

void SecureClear(std::vector<std::uint8_t>* value) {
  if (value == nullptr) {
    return;
  }
  volatile std::uint8_t* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t index = 0; index < value->size(); ++index) {
    bytes[index] = 0;
  }
  value->clear();
}

struct SensitiveString {
  ~SensitiveString() { SecureClear(&value); }
  std::string value;
};

struct SensitiveWideString {
  ~SensitiveWideString() { SecureClear(&value); }
  std::wstring value;
};

bool IsUtf8Continuation(unsigned char value) {
  return (value & 0xC0U) == 0x80U;
}

SensitiveString VisibleTextWindow(
    const RobloxTextOverlayPresentation& presentation, std::size_t* caret_byte,
    std::size_t* selection_begin_byte, std::size_t* selection_end_byte) {
  SensitiveString result;
  const std::string& text = presentation.display_utf8;
  const std::size_t caret = std::min(presentation.caret_utf8_byte, text.size());
  const std::size_t selection_begin =
      std::min(presentation.selection_begin_utf8_byte, text.size());
  const std::size_t selection_end =
      std::min(presentation.selection_end_utf8_byte, text.size());
  if (text.size() <= kMaximumRasterTextBytes) {
    result.value = text;
    *caret_byte = caret;
    *selection_begin_byte = std::min(selection_begin, selection_end);
    *selection_end_byte = std::max(selection_begin, selection_end);
    return result;
  }

  const std::size_t half = kMaximumRasterTextBytes / 2;
  std::size_t begin = caret > half ? caret - half : 0;
  std::size_t end = std::min(text.size(), begin + kMaximumRasterTextBytes);
  if (end - begin < kMaximumRasterTextBytes && begin != 0) {
    begin = end - kMaximumRasterTextBytes;
  }
  while (begin < text.size() &&
         IsUtf8Continuation(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && end < text.size() &&
         IsUtf8Continuation(static_cast<unsigned char>(text[end]))) {
    --end;
  }
  result.value.assign(text.data() + begin, end - begin);
  *caret_byte = std::min(caret - begin, result.value.size());
  const auto map_to_window = [begin, end](std::size_t byte) {
    if (byte <= begin) {
      return std::size_t{0};
    }
    if (byte >= end) {
      return end - begin;
    }
    return byte - begin;
  };
  *selection_begin_byte =
      map_to_window(std::min(selection_begin, selection_end));
  *selection_end_byte = map_to_window(std::max(selection_begin, selection_end));
  return result;
}

struct TextColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 0;
};

TextColor ResolveTextColor(std::int32_t packed_color) {
  const std::uint32_t packed = static_cast<std::uint32_t>(packed_color);
  const std::uint8_t alpha = static_cast<std::uint8_t>(packed >> 24U);
  if (alpha == 0) {
    return {212, 216, 225, 255};
  }
  return {static_cast<std::uint8_t>(packed >> 16U),
          static_cast<std::uint8_t>(packed >> 8U),
          static_cast<std::uint8_t>(packed), alpha};
}

void DecodeUtf8(const std::string& text, std::wstring* wide,
                std::vector<std::uint32_t>* index_at_byte) {
  wide->clear();
  wide->reserve(text.size());
  index_at_byte->assign(text.size() + 1, 0);
  std::size_t offset = 0;
  while (offset < text.size()) {
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    std::uint32_t code_point = 0xFFFD;
    std::size_t length = 1;
    if (lead < 0x80U) {
      code_point = lead;
    } else if ((lead & 0xE0U) == 0xC0U) {
      code_point = lead & 0x1FU;
      length = 2;
    } else if ((lead & 0xF0U) == 0xE0U) {
      code_point = lead & 0x0FU;
      length = 3;
    } else if ((lead & 0xF8U) == 0xF0U) {
      code_point = lead & 0x07U;
      length = 4;
    }
    if (length > 1) {
      if (offset + length > text.size()) {
        code_point = 0xFFFD;
        length = 1;
      } else {
        for (std::size_t index = 1; index < length; ++index) {
          const unsigned char next =
              static_cast<unsigned char>(text[offset + index]);
          if (!IsUtf8Continuation(next)) {
            code_point = 0xFFFD;
            length = index;
            break;
          }
          code_point = (code_point << 6U) | (next & 0x3FU);
        }
      }
    }
    const std::uint32_t index = static_cast<std::uint32_t>(wide->size());
    for (std::size_t byte = 0; byte < length; ++byte) {
      (*index_at_byte)[offset + byte] = index;
    }
    if (code_point >= 0x10000U && code_point <= 0x10FFFFU) {
      const std::uint32_t value = code_point - 0x10000U;
      wide->push_back(static_cast<wchar_t>(0xD800U + (value >> 10U)));
      wide->push_back(static_cast<wchar_t>(0xDC00U + (value & 0x3FFU)));
    } else if (code_point > 0x10FFFFU ||
               (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      wide->push_back(L'\xFFFD');
    } else {
      wide->push_back(static_cast<wchar_t>(code_point));
    }
    offset += length;
  }
  (*index_at_byte)[text.size()] = static_cast<std::uint32_t>(wide->size());
  for (wchar_t& unit : *wide) {
    if (unit == L'\r' || unit == L'\v' || unit == L'\f' || unit == L'\x85' ||
        unit == L'\x2028' || unit == L'\x2029') {
      unit = L' ';
    }
  }
}

std::wstring Widen(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                         static_cast<int>(utf8.size()),
                                         nullptr, 0);
  if (length <= 0) {
    return {};
  }
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      wide.data(), length);
  return wide;
}

Status RasterizationFailure(const char* operation, HRESULT result = S_OK) {
  std::string message = operation != nullptr ? operation : "text rasterization";
  if (FAILED(result)) {
    char detail[32];
    std::snprintf(detail, sizeof(detail), ": HRESULT 0x%08lX",
                  static_cast<unsigned long>(result));
    message.append(detail);
  }
  return Status::Error(StatusCode::kPlatformError, std::move(message));
}

struct FontFace {
  ComPtr<IDWriteFontCollection> collection;
  std::wstring family;
  DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
  DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
  DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
  DWRITE_FONT_METRICS metrics{};
};

bool DescribeFace(IDWriteFontFamily* family, IDWriteFont* font,
                  FontFace* face) {
  ComPtr<IDWriteLocalizedStrings> names;
  if (FAILED(family->GetFamilyNames(&names)) || names->GetCount() == 0) {
    return false;
  }
  UINT32 index = 0;
  BOOL exists = FALSE;
  if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || !exists) {
    index = 0;
  }
  UINT32 length = 0;
  if (FAILED(names->GetStringLength(index, &length))) {
    return false;
  }
  std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
  if (FAILED(names->GetString(index, name.data(), length + 1))) {
    return false;
  }
  name.resize(length);
  face->family = std::move(name);
  face->weight = font->GetWeight();
  face->style = font->GetStyle();
  face->stretch = font->GetStretch();
  font->GetMetrics(&face->metrics);
  return face->metrics.designUnitsPerEm != 0;
}

bool LoadFileFace(IDWriteFactory* factory, const std::wstring& path,
                  FontFace* face) {
  ComPtr<IDWriteFactory5> factory5;
  if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
    return false;
  }
  ComPtr<IDWriteFontFile> file;
  ComPtr<IDWriteFontSetBuilder1> builder;
  ComPtr<IDWriteFontSet> set;
  ComPtr<IDWriteFontCollection1> collection;
  if (FAILED(factory5->CreateFontFileReference(path.c_str(), nullptr,
                                               &file)) ||
      FAILED(factory5->CreateFontSetBuilder(builder.GetAddressOf())) ||
      FAILED(builder->AddFontFile(file.Get())) ||
      FAILED(builder->CreateFontSet(&set)) ||
      FAILED(factory5->CreateFontCollectionFromFontSet(set.Get(),
                                                       &collection)) ||
      collection->GetFontFamilyCount() == 0) {
    return false;
  }
  ComPtr<IDWriteFontFamily> family;
  ComPtr<IDWriteFont> font;
  if (FAILED(collection->GetFontFamily(0, &family)) ||
      family->GetFontCount() == 0 || FAILED(family->GetFont(0, &font))) {
    return false;
  }
  face->collection = collection;
  return DescribeFace(family.Get(), font.Get(), face);
}

bool LoadSystemFace(IDWriteFactory* factory, const wchar_t* family_name,
                    FontFace* face) {
  ComPtr<IDWriteFontCollection> system;
  if (FAILED(factory->GetSystemFontCollection(&system, FALSE))) {
    return false;
  }
  UINT32 index = 0;
  BOOL exists = FALSE;
  if (FAILED(system->FindFamilyName(family_name, &index, &exists)) ||
      !exists) {
    return false;
  }
  ComPtr<IDWriteFontFamily> family;
  ComPtr<IDWriteFont> font;
  if (FAILED(system->GetFontFamily(index, &family)) ||
      FAILED(family->GetFirstMatchingFont(
          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
          DWRITE_FONT_STYLE_NORMAL, &font))) {
    return false;
  }
  face->collection = system;
  return DescribeFace(family.Get(), font.Get(), face);
}

class RasterResources final {
 public:
  static RasterResources& Get() {
    static RasterResources resources;
    return resources;
  }

  Status Ensure(ComPtr<IDWriteFactory>* dwrite, ComPtr<ID2D1Factory>* d2d) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (dwrite_ == nullptr) {
      const HRESULT result = DWriteCreateFactory(
          DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown**>(dwrite_.ReleaseAndGetAddressOf()));
      if (FAILED(result)) {
        dwrite_.Reset();
        return RasterizationFailure("unable to create the DirectWrite factory",
                                    result);
      }
    }
    if (d2d_ == nullptr) {
      const HRESULT result = D2D1CreateFactory(
          D2D1_FACTORY_TYPE_MULTI_THREADED, d2d_.ReleaseAndGetAddressOf());
      if (FAILED(result)) {
        d2d_.Reset();
        return RasterizationFailure("unable to create the Direct2D factory",
                                    result);
      }
    }
    *dwrite = dwrite_;
    *d2d = d2d_;
    return Status::Ok();
  }

  bool Face(const std::wstring& path, FontFace* face) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (dwrite_ == nullptr) {
      return false;
    }
    const auto cached = faces_.find(path);
    if (cached != faces_.end()) {
      if (!cached->second.first) {
        return false;
      }
      *face = cached->second.second;
      return true;
    }
    FontFace loaded;
    const bool ok = path.empty()
                        ? LoadSystemFace(dwrite_.Get(), kFallbackFamily, &loaded)
                        : LoadFileFace(dwrite_.Get(), path, &loaded);
    faces_.emplace(path, std::make_pair(ok, loaded));
    if (ok) {
      *face = loaded;
    }
    return ok;
  }

 private:
  RasterResources() = default;

  std::mutex mutex_;
  ComPtr<IDWriteFactory> dwrite_;
  ComPtr<ID2D1Factory> d2d_;
  std::map<std::wstring, std::pair<bool, FontFace>> faces_;
};

struct LineLayout {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  ComPtr<IDWriteTextLayout> layout;
  float width = 0.0F;
};

HRESULT CreateLineLayout(IDWriteFactory* factory, IDWriteTextFormat* format,
                         const std::wstring& text, std::uint32_t begin,
                         std::uint32_t end, LineLayout* line) {
  line->begin = begin;
  line->end = end;
  const HRESULT result = factory->CreateTextLayout(
      text.data() + begin, end - begin, format, kUnboundedLayoutWidth,
      kUnboundedLayoutWidth, line->layout.ReleaseAndGetAddressOf());
  if (FAILED(result)) {
    return result;
  }
  DWRITE_TEXT_METRICS metrics{};
  const HRESULT measured = line->layout->GetMetrics(&metrics);
  if (FAILED(measured)) {
    return measured;
  }
  line->width = metrics.widthIncludingTrailingWhitespace;
  return S_OK;
}

HRESULT WrapParagraph(IDWriteFactory* factory, IDWriteTextFormat* format,
                      const std::wstring& text, std::uint32_t begin,
                      std::uint32_t end, float available,
                      std::vector<std::pair<std::uint32_t, std::uint32_t>>*
                          ranges) {
  ComPtr<IDWriteTextLayout> paragraph;
  HRESULT result = factory->CreateTextLayout(
      text.data() + begin, end - begin, format, kUnboundedLayoutWidth,
      kUnboundedLayoutWidth, &paragraph);
  if (FAILED(result)) {
    return result;
  }
  UINT32 count = 0;
  result = paragraph->GetClusterMetrics(nullptr, 0, &count);
  if (result != E_NOT_SUFFICIENT_BUFFER && FAILED(result)) {
    return result;
  }
  std::vector<DWRITE_CLUSTER_METRICS> clusters(count);
  if (count != 0) {
    result = paragraph->GetClusterMetrics(clusters.data(), count, &count);
    if (FAILED(result)) {
      return result;
    }
  }
  std::vector<std::uint32_t> starts(clusters.size() + 1, begin);
  for (std::size_t index = 0; index < clusters.size(); ++index) {
    starts[index + 1] = starts[index] + clusters[index].length;
  }
  const auto width_between = [&clusters](std::size_t first, std::size_t last) {
    float width = 0.0F;
    for (std::size_t index = first; index < last; ++index) {
      width += clusters[index].width;
    }
    return width;
  };

  std::size_t line_first = 0;
  float line_width = 0.0F;
  std::ptrdiff_t last_break = -1;
  for (std::size_t index = 0; index < clusters.size(); ++index) {
    const DWRITE_CLUSTER_METRICS& cluster = clusters[index];
    while (!cluster.isWhitespace && index > line_first &&
           line_width + cluster.width > available) {
      const std::size_t next_first =
          last_break >= static_cast<std::ptrdiff_t>(line_first)
              ? static_cast<std::size_t>(last_break) + 1
              : index;
      ranges->emplace_back(starts[line_first], starts[next_first]);
      line_first = next_first;
      line_width = width_between(line_first, index);
      last_break = -1;
    }
    line_width += cluster.width;
    if (cluster.canWrapLineAfter) {
      last_break = static_cast<std::ptrdiff_t>(index);
    }
  }
  ranges->emplace_back(starts[line_first], end);
  return S_OK;
}

struct TextBytePosition {
  int x = 0;
  int y = 0;
  int height = 0;
};

TextBytePosition PositionOf(const std::vector<LineLayout>& lines,
                            std::uint32_t index, int line_height) {
  std::size_t line = 0;
  for (std::size_t candidate = 0; candidate < lines.size(); ++candidate) {
    if (lines[candidate].begin <= index) {
      line = candidate;
    } else {
      break;
    }
  }
  const LineLayout& layout = lines[line];
  float x = layout.width;
  if (index < layout.end && layout.layout != nullptr) {
    FLOAT point_x = 0.0F;
    FLOAT point_y = 0.0F;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (SUCCEEDED(layout.layout->HitTestTextPosition(
            index - layout.begin, FALSE, &point_x, &point_y, &metrics))) {
      x = point_x;
    }
  }
  return {static_cast<int>(std::lround(x)),
          static_cast<int>(line) * line_height, line_height};
}

class CoverageBitmap final {
 public:
  CoverageBitmap() = default;
  ~CoverageBitmap() {
    if (bits_ != nullptr) {
      SecureZeroMemory(bits_, static_cast<std::size_t>(width_) *
                                  static_cast<std::size_t>(height_) * 4);
    }
    if (dc_ != nullptr && previous_ != nullptr) {
      SelectObject(dc_, previous_);
    }
    if (bitmap_ != nullptr) {
      DeleteObject(bitmap_);
    }
    if (dc_ != nullptr) {
      DeleteDC(dc_);
    }
  }
  CoverageBitmap(const CoverageBitmap&) = delete;
  CoverageBitmap& operator=(const CoverageBitmap&) = delete;

  bool Create(int width, int height) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    dc_ = CreateCompatibleDC(nullptr);
    if (dc_ == nullptr) {
      return false;
    }
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap_ == nullptr || bits == nullptr) {
      return false;
    }
    previous_ = SelectObject(dc_, bitmap_);
    bits_ = static_cast<std::uint8_t*>(bits);
    width_ = width;
    height_ = height;
    std::memset(bits_, 0, static_cast<std::size_t>(width) *
                              static_cast<std::size_t>(height) * 4);
    return true;
  }

  HDC dc() const { return dc_; }
  std::uint8_t coverage(int x, int y) const {
    return bits_[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(x)) *
                     4 +
                 3];
  }

 private:
  HDC dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ previous_ = nullptr;
  std::uint8_t* bits_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

void SetRgbaPixel(std::vector<std::uint8_t>* pixels, int width, int x, int y,
              std::uint8_t red, std::uint8_t green, std::uint8_t blue,
              std::uint8_t alpha) {
  if (pixels == nullptr || x < 0 || y < 0 || x >= width) {
    return;
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
       static_cast<std::size_t>(x)) *
      4;
  if (offset + 3 >= pixels->size()) {
    return;
  }
  (*pixels)[offset] = red;
  (*pixels)[offset + 1] = green;
  (*pixels)[offset + 2] = blue;
  (*pixels)[offset + 3] = alpha;
}

}

RobloxTextSurfaceOverlay::~RobloxTextSurfaceOverlay() { (void)Shutdown(); }

Status RobloxTextSurfaceOverlay::Initialize(
    RobloxTextOverlayViewport viewport) {
  if (!viewport.valid()) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "text surface overlay viewport is invalid");
  }
  std::lock_guard<std::mutex> active_lock(g_active_overlay_mutex);
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_ || g_active_overlay != nullptr) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "text surface overlay is already initialized");
  }
  viewport_ = viewport;
  failure_ = Status::Ok();
  initialized_ = true;
  g_active_overlay = this;
  g_overlay_may_present.store(false, std::memory_order_release);
  return Status::Ok();
}

Status RobloxTextSurfaceOverlay::Shutdown() {
  std::lock_guard<std::mutex> active_lock(g_active_overlay_mutex);
  std::lock_guard<std::mutex> lock(mutex_);
  const bool owned_active_overlay = g_active_overlay == this;
  if (owned_active_overlay) {
    g_active_overlay = nullptr;
  }
  state_.Reset();
  ClearFrameLocked();
  viewport_ = {};
  initialized_ = false;
  change_callback_ = nullptr;
  change_context_ = nullptr;
  if (owned_active_overlay) {
    g_overlay_may_present.store(false, std::memory_order_release);
  }
  return failure_;
}

Status RobloxTextSurfaceOverlay::UpdateViewport(
    RobloxTextOverlayViewport viewport) {
  if (!viewport.valid()) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "text surface overlay viewport is invalid");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "text surface overlay is not initialized");
  }
  viewport_ = viewport;
  return Status::Ok();
}

Status RobloxTextSurfaceOverlay::SetRasterScale(float scale) {
  if (!std::isfinite(scale) || scale < kMinimumRasterScale ||
      scale > kMaximumRasterScale) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "text surface overlay raster scale is invalid");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (scale == raster_scale_) {
    return Status::Ok();
  }
  raster_scale_ = scale;
  ++state_revision_;
  if (state_revision_ == 0) {
    ++state_revision_;
  }
  if (change_callback_ != nullptr) {
    change_callback_(change_context_);
  }
  return Status::Ok();
}

void RobloxTextSurfaceOverlay::SetChangeCallback(ChangeCallback callback,
                                                 void* context) {
  std::lock_guard<std::mutex> lock(mutex_);
  change_callback_ = callback;
  change_context_ = callback != nullptr ? context : nullptr;
}

RobloxTextDisplaySink RobloxTextSurfaceOverlay::sink() {
  return {this, &RobloxTextSurfaceOverlay::UpdateCallback};
}

void RobloxTextSurfaceOverlay::UpdateCallback(
    void* context, const RobloxTextDisplayUpdate& update) {
  if (context != nullptr) {
    static_cast<RobloxTextSurfaceOverlay*>(context)->ApplyUpdate(update);
  }
}

void RobloxTextSurfaceOverlay::ApplyUpdate(
    const RobloxTextDisplayUpdate& update) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_) {
    return;
  }
  bool changed = false;
  Status status = state_.Apply(update, viewport_, &changed);
  if (!status.ok()) {
    RecordFailureLocked(std::move(status));
    return;
  }
  if (!changed) {
    return;
  }
  ++state_revision_;
  if (state_revision_ == 0) {
    ++state_revision_;
  }
  const RobloxTextOverlayPresentation& presentation = state_.presentation();
  const bool drawable = presentation.visible && presentation.geometry.valid();
  if (!drawable) {
    ClearFrameLocked();
  }
  g_overlay_may_present.store(drawable, std::memory_order_release);
  if (change_callback_ != nullptr) {
    change_callback_(change_context_);
  }
}

bool RobloxTextSurfaceOverlay::QueryFrame(MocktailTextOverlayFrameInfo* frame) {
  if (frame == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  *frame = {};
  frame->revision = state_revision_;
  if (!initialized_) {
    return false;
  }
  const RobloxTextOverlayPresentation& presentation = state_.presentation();
  if (!presentation.visible) {
    return true;
  }
  if (!presentation.geometry.valid()) {
    return true;
  }
  if (raster_revision_ != state_revision_) {
    Status status = RasterizeLocked();
    if (!status.ok()) {
      RecordFailureLocked(std::move(status));
      return false;
    }
  }
  if (rgba_.empty() || !presentation.geometry.valid()) {
    return false;
  }
  frame->visible = 1;
  frame->coordinate_width =
      static_cast<std::uint32_t>(viewport_.coordinate_width());
  frame->coordinate_height =
      static_cast<std::uint32_t>(viewport_.coordinate_height());
  frame->x = presentation.geometry.x;
  frame->y = presentation.geometry.y -
             static_cast<std::int32_t>(std::lround(
                 static_cast<float>(raster_rows_above_) / raster_scale_));
  frame->width = static_cast<std::uint32_t>(std::max(
      1L, std::lround(static_cast<float>(presentation.geometry.width) *
                      raster_scale_)));
  frame->height = raster_height_ != 0
                      ? raster_height_
                      : static_cast<std::uint32_t>(std::max(
                            1L, std::lround(static_cast<float>(
                                                presentation.geometry.height) *
                                            raster_scale_)));
  frame->row_bytes = frame->width * 4;
  frame->rgba_bytes = rgba_.size();
  return true;
}

bool RobloxTextSurfaceOverlay::CopyFrame(std::uint64_t revision, void* rgba,
                                         std::size_t rgba_capacity) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_ || revision == 0 || revision != raster_revision_ ||
      rgba == nullptr || rgba_capacity < rgba_.size() || rgba_.empty()) {
    return false;
  }
  std::memcpy(rgba, rgba_.data(), rgba_.size());
  return true;
}

Status RobloxTextSurfaceOverlay::RasterizeLocked() {
  const RobloxTextOverlayPresentation& presentation = state_.presentation();
  if (!presentation.visible || !presentation.geometry.valid()) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "visible text surface geometry is unavailable");
  }
  const float scale = raster_scale_;
  const auto units = [scale](float value) {
    return std::max(1, static_cast<int>(std::lround(value * scale)));
  };
  const std::uint64_t width = static_cast<std::uint64_t>(
      units(static_cast<float>(presentation.geometry.width)));
  const std::uint64_t native_height = static_cast<std::uint64_t>(
      units(static_cast<float>(presentation.geometry.height)));
  if (width * native_height > kMaximumOverlayPixels ||
      width * native_height > std::numeric_limits<std::size_t>::max() / 4) {
    return Status::Error(StatusCode::kUnsupported,
                         "text surface dimensions exceed the safety limit");
  }

  std::vector<std::uint8_t> candidate;
  const int frame_width = static_cast<int>(width);
  int frame_height = static_cast<int>(native_height);
  const int clip_left = 0;
  const int clip_right = frame_width;
  const int clip_top = 0;
  int clip_bottom = frame_height;
  const int available_width = std::max(1, clip_right - clip_left);
  const bool wrapped_layout =
      presentation.multiline || presentation.text_wrapped;

  ComPtr<IDWriteFactory> dwrite;
  ComPtr<ID2D1Factory> d2d;
  Status status = RasterResources::Get().Ensure(&dwrite, &d2d);
  if (!status.ok()) {
    SecureClear(&candidate);
    return status;
  }
  const RobloxTextFontSelection font_selection =
      ResolveRobloxTextFont(presentation.font);
  const float native_point_size =
      std::isfinite(presentation.font_size) ? presentation.font_size : 0.0F;
  const float point_size =
      native_point_size > 0.0F
          ? std::clamp(native_point_size * font_selection.size_scale, 1.0F,
                       512.0F)
          : std::clamp(static_cast<float>(presentation.geometry.height) * 0.38F,
                       12.0F, 28.0F);
  const float em_size = point_size * scale;

  FontFace face;
  bool have_face = false;
  const char* configured = std::getenv("MOCKTAIL_TEXT_FONT");
  if (configured != nullptr && configured[0] != '\0') {
    have_face = RasterResources::Get().Face(Widen(configured), &face);
  }
  if (!have_face && !font_selection.primary_file.empty()) {
    have_face = RasterResources::Get().Face(
        Widen(font_selection.primary_file), &face);
  }
  if (!have_face) {
    have_face = RasterResources::Get().Face(std::wstring(), &face);
  }
  if (!have_face) {
    SecureClear(&candidate);
    return RasterizationFailure("unable to open a Roblox or fallback font");
  }

  const float design_units =
      static_cast<float>(face.metrics.designUnitsPerEm);
  const float ascent =
      static_cast<float>(face.metrics.ascent) * em_size / design_units;
  const float descent =
      static_cast<float>(face.metrics.descent) * em_size / design_units;
  const int line_height =
      std::max(1, static_cast<int>(std::ceil(ascent + descent)));

  const int native_frame_height = frame_height;
  if (!wrapped_layout && line_height > frame_height) {
    frame_height = line_height;
    clip_bottom = frame_height;
  }
  if (width * static_cast<std::uint64_t>(frame_height) > kMaximumOverlayPixels) {
    return Status::Error(StatusCode::kUnsupported,
                         "text surface dimensions exceed the safety limit");
  }
  candidate.assign(static_cast<std::size_t>(width) *
                       static_cast<std::size_t>(frame_height) * 4,
                   0);

  ComPtr<IDWriteTextFormat> format;
  HRESULT result = dwrite->CreateTextFormat(
      face.family.c_str(), face.collection.Get(), face.weight, face.style,
      face.stretch, em_size, L"en-us", &format);
  if (SUCCEEDED(result)) {
    result = format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }
  if (SUCCEEDED(result)) {
    result = format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
                                    static_cast<float>(line_height), ascent);
  }
  if (FAILED(result)) {
    SecureClear(&candidate);
    return RasterizationFailure("unable to create the text format", result);
  }

  std::size_t caret_byte = 0;
  std::size_t selection_begin_byte = 0;
  std::size_t selection_end_byte = 0;
  SensitiveString text = VisibleTextWindow(
      presentation, &caret_byte, &selection_begin_byte, &selection_end_byte);
  SensitiveWideString wide;
  std::vector<std::uint32_t> index_at_byte;
  DecodeUtf8(text.value, &wide.value, &index_at_byte);
  const std::uint32_t caret_index = index_at_byte[caret_byte];
  const std::uint32_t selection_begin_index =
      index_at_byte[selection_begin_byte];
  const std::uint32_t selection_end_index = index_at_byte[selection_end_byte];

  std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
  {
    const std::uint32_t length = static_cast<std::uint32_t>(wide.value.size());
    std::uint32_t paragraph_begin = 0;
    for (std::uint32_t index = 0; index <= length && SUCCEEDED(result);
         ++index) {
      if (index != length && wide.value[index] != L'\n') {
        continue;
      }
      if (wrapped_layout && index > paragraph_begin) {
        result = WrapParagraph(dwrite.Get(), format.Get(), wide.value,
                               paragraph_begin, index,
                               static_cast<float>(available_width), &ranges);
      } else {
        ranges.emplace_back(paragraph_begin, index);
      }
      paragraph_begin = index + 1;
    }
  }
  std::vector<LineLayout> lines(ranges.size());
  for (std::size_t index = 0; index < ranges.size() && SUCCEEDED(result);
       ++index) {
    result = CreateLineLayout(dwrite.Get(), format.Get(), wide.value,
                              ranges[index].first, ranges[index].second,
                              &lines[index]);
  }
  if (FAILED(result) || lines.empty()) {
    SecureClear(&candidate);
    return RasterizationFailure("unable to lay out Unicode text", result);
  }

  int text_width = 0;
  for (const LineLayout& line : lines) {
    text_width = std::max(text_width,
                          static_cast<int>(std::ceil(line.width)));
  }
  const int text_height = line_height * static_cast<int>(lines.size());
  const TextBytePosition caret_position =
      PositionOf(lines, caret_index, line_height);

  const int caret_clearance = units(3.0F);
  int text_offset = 0;
  if (!wrapped_layout && text_width <= available_width) {
    if (presentation.x_alignment == 1) {
      text_offset = available_width - text_width;
    } else if (presentation.x_alignment == 2) {
      text_offset = (available_width - text_width) / 2;
    }
  } else if (!wrapped_layout &&
             caret_position.x > available_width - caret_clearance) {
    text_offset = available_width - caret_clearance - caret_position.x;
  }
  const auto line_offset = [&](std::size_t line) {
    if (!wrapped_layout) {
      return text_offset;
    }
    const int line_width = static_cast<int>(std::ceil(lines[line].width));
    if (presentation.x_alignment == 1) {
      return available_width - line_width;
    }
    if (presentation.x_alignment == 2) {
      return (available_width - line_width) / 2;
    }
    return 0;
  };
  const auto line_of = [&lines](std::uint32_t index) {
    std::size_t line = 0;
    for (std::size_t candidate_line = 0; candidate_line < lines.size();
         ++candidate_line) {
      if (lines[candidate_line].begin <= index) {
        line = candidate_line;
      } else {
        break;
      }
    }
    return line;
  };
  int text_y = clip_top;
  if (presentation.y_alignment == 1) {
    text_y = std::max(clip_top, (frame_height - text_height) / 2);
  } else if (presentation.y_alignment == 2) {
    text_y = std::max(clip_top, clip_bottom - text_height);
  }

  CoverageBitmap coverage;
  if (!coverage.Create(frame_width, frame_height)) {
    SecureClear(&candidate);
    return RasterizationFailure("unable to create the text bitmap",
                                HRESULT_FROM_WIN32(GetLastError()));
  }
  {
    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0F, 96.0F);
    ComPtr<ID2D1DCRenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> white;
    result = d2d->CreateDCRenderTarget(&properties, &target);
    const RECT bounds{0, 0, frame_width, frame_height};
    if (SUCCEEDED(result)) {
      result = target->BindDC(coverage.dc(), &bounds);
    }
    if (SUCCEEDED(result)) {
      result = target->CreateSolidColorBrush(D2D1::ColorF(1.0F, 1.0F, 1.0F, 1.0F),
                                             &white);
    }
    if (SUCCEEDED(result)) {
      target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
      target->BeginDraw();
      target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
      for (std::size_t line = 0; line < lines.size(); ++line) {
        const int line_y = text_y + static_cast<int>(line) * line_height;
        if (line_y >= clip_bottom || line_y + line_height <= clip_top) {
          continue;
        }
        target->DrawTextLayout(
            D2D1::Point2F(static_cast<float>(clip_left + line_offset(line)),
                          static_cast<float>(line_y)),
            lines[line].layout.Get(), white.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE);
      }
      result = target->EndDraw();
    }
    GdiFlush();
  }
  if (FAILED(result)) {
    SecureClear(&candidate);
    return RasterizationFailure("unable to rasterize Unicode text", result);
  }

  const TextColor text_color = ResolveTextColor(presentation.text_color);
  const auto fill_highlight = [&](int left, int top, int right, int bottom) {
    const int bounded_left = std::clamp(left, clip_left, clip_right);
    const int bounded_right = std::clamp(right, bounded_left, clip_right);
    const int bounded_top = std::clamp(top, clip_top, clip_bottom);
    const int bounded_bottom = std::clamp(bottom, bounded_top, clip_bottom);
    for (int y = bounded_top; y < bounded_bottom; ++y) {
      for (int x = bounded_left; x < bounded_right; ++x) {
        SetRgbaPixel(&candidate, frame_width, x, y, 65, 132, 228, 112);
      }
    }
  };
  if (selection_begin_index != selection_end_index) {
    for (std::size_t line = 0; line < lines.size(); ++line) {
      const std::uint32_t first =
          std::max(selection_begin_index, lines[line].begin);
      const std::uint32_t last = std::min(selection_end_index, lines[line].end);
      if (first >= last) {
        continue;
      }
      const int begin_x = PositionOf(lines, first, line_height).x;
      const int end_x = line_of(last) == line
                            ? PositionOf(lines, last, line_height).x
                            : static_cast<int>(std::lround(lines[line].width));
      const int line_y = text_y + static_cast<int>(line) * line_height;
      fill_highlight(clip_left + line_offset(line) + std::min(begin_x, end_x),
                     line_y,
                     clip_left + line_offset(line) + std::max(begin_x, end_x),
                     line_y + line_height);
    }
  }
  for (int y = clip_top; y < clip_bottom; ++y) {
    for (int x = clip_left; x < clip_right; ++x) {
      const std::uint32_t covered = coverage.coverage(x, y);
      if (covered == 0) {
        continue;
      }
      const std::uint32_t alpha = (covered * text_color.a + 127U) / 255U;
      if (alpha == 0) {
        continue;
      }
      SetRgbaPixel(&candidate, frame_width, x, y, text_color.r, text_color.g,
               text_color.b, static_cast<std::uint8_t>(alpha));
    }
  }
  const int caret_width = units(1.0F);
  const int caret_line = static_cast<int>(line_of(caret_index));
  const int caret_screen_x = std::clamp(
      clip_left + line_offset(static_cast<std::size_t>(caret_line)) +
          caret_position.x,
      clip_left, std::max(clip_left, clip_right - 1 - caret_width));
  const int caret_line_height =
      caret_position.height > 0 ? caret_position.height : line_height;
  const int caret_height = std::min(std::max(units(12.0F), caret_line_height),
                                    clip_bottom - clip_top);
  const int caret_y =
      std::clamp(text_y + caret_position.y, clip_top,
                 std::max(clip_top, clip_bottom - caret_height));
  for (int y = caret_y; y < caret_y + caret_height; ++y) {
    for (int x = caret_screen_x; x < caret_screen_x + caret_width; ++x) {
      SetRgbaPixel(&candidate, frame_width, x, y, text_color.r, text_color.g,
               text_color.b, text_color.a);
    }
  }

  SecureClear(&rgba_);
  rgba_ = std::move(candidate);
  raster_revision_ = state_revision_;
  raster_height_ = static_cast<std::uint32_t>(frame_height);
  raster_rows_above_ =
      static_cast<std::uint32_t>((frame_height - native_frame_height) / 2);
  if (!ready_logged_) {
    ready_logged_ = true;
    std::fprintf(stderr,
                 "  [input] TextBox overlay raster ready "
                 "(DirectWrite/Direct2D)\n");
  }
  return Status::Ok();
}

void RobloxTextSurfaceOverlay::ClearFrameLocked() {
  SecureClear(&rgba_);
  raster_revision_ = 0;
  raster_height_ = 0;
  raster_rows_above_ = 0;
}

void RobloxTextSurfaceOverlay::RecordFailureLocked(Status status) {
  if (status.ok()) {
    return;
  }
  failure_ = std::move(status);
  if (!failure_logged_) {
    failure_logged_ = true;
    std::fprintf(stderr, "  [input] TextBox overlay raster failed: %s\n",
                 failure_.message().c_str());
  }
}

}
}

extern "C" bool mocktail_text_overlay_may_present() {
  return mocktail::runtime::g_overlay_may_present.load(
      std::memory_order_acquire);
}

extern "C" bool mocktail_text_overlay_query(
    MocktailTextOverlayFrameInfo* frame) {
  std::lock_guard<std::mutex> lock(mocktail::runtime::g_active_overlay_mutex);
  return mocktail::runtime::g_active_overlay != nullptr &&
         mocktail::runtime::g_active_overlay->QueryFrame(frame);
}

extern "C" bool mocktail_text_overlay_copy(std::uint64_t revision, void* rgba,
                                           std::size_t rgba_capacity) {
  std::lock_guard<std::mutex> lock(mocktail::runtime::g_active_overlay_mutex);
  return mocktail::runtime::g_active_overlay != nullptr &&
         mocktail::runtime::g_active_overlay->CopyFrame(revision, rgba,
                                                        rgba_capacity);
}
