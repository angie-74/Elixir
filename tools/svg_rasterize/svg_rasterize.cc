// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <d2d1_3.h>
#include <d3d11.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

#define CHECK_HR(expr)                                                                   \
  do {                                                                                   \
    const HRESULT check_hr_ = (expr);                                                    \
    if (FAILED(check_hr_)) {                                                             \
      std::fprintf(stderr, "svg_rasterize: %s failed (0x%08lx)\n", #expr,               \
                   static_cast<unsigned long>(check_hr_));                               \
      std::exit(2);                                                                      \
    }                                                                                    \
  } while (0)

struct Renderer {
  ComPtr<ID3D11Device> d3d;
  ComPtr<ID2D1Factory7> factory;
  ComPtr<ID2D1Device6> device;
  ComPtr<ID2D1DeviceContext6> context;
  ComPtr<IWICImagingFactory> wic;

  Renderer() {
    CHECK_HR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               nullptr, 0, D3D11_SDK_VERSION, &d3d, nullptr, nullptr));
    ComPtr<IDXGIDevice> dxgi;
    CHECK_HR(d3d.As(&dxgi));
    D2D1_FACTORY_OPTIONS options{};
    CHECK_HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7), &options,
                               reinterpret_cast<void**>(factory.GetAddressOf())));
    CHECK_HR(factory->CreateDevice(dxgi.Get(), &device));
    CHECK_HR(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context));
    CHECK_HR(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)));
  }

  std::vector<std::uint8_t> Render(const std::wstring& svg_path, UINT width, UINT height) {
    ComPtr<IStream> stream;
    CHECK_HR(SHCreateStreamOnFileEx(svg_path.c_str(), STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream));
    ComPtr<ID2D1SvgDocument> document;
    CHECK_HR(context->CreateSvgDocument(stream.Get(), D2D1::SizeF(static_cast<float>(width),
                                                                 static_cast<float>(height)),
                                        &document));
    ComPtr<ID2D1SvgElement> root;
    document->GetRoot(&root);
    root->SetAttributeValue(L"width", D2D1_SVG_LENGTH{static_cast<float>(width), D2D1_SVG_LENGTH_UNITS_NUMBER});
    root->SetAttributeValue(L"height", D2D1_SVG_LENGTH{static_cast<float>(height), D2D1_SVG_LENGTH_UNITS_NUMBER});

    const D2D1_BITMAP_PROPERTIES1 target_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> target;
    CHECK_HR(context->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, target_properties, &target));
    context->SetTarget(target.Get());
    context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0, 0, 0, 0));
    context->DrawSvgDocument(document.Get());
    CHECK_HR(context->EndDraw());

    const D2D1_BITMAP_PROPERTIES1 readback_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> readback;
    CHECK_HR(context->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, readback_properties, &readback));
    CHECK_HR(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
    D2D1_MAPPED_RECT mapped{};
    CHECK_HR(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
    for (UINT y = 0; y < height; ++y) {
      const std::uint8_t* row = mapped.bits + static_cast<std::size_t>(y) * mapped.pitch;
      for (UINT x = 0; x < width; ++x) {
        const std::uint8_t* in = row + x * 4;
        std::uint8_t* out = &pixels[(static_cast<std::size_t>(y) * width + x) * 4];
        const unsigned alpha = in[3];
        for (int c = 0; c < 3; ++c) {
          out[c] = alpha == 0 ? 0 : static_cast<std::uint8_t>((in[c] * 255u + alpha / 2) / alpha);
        }
        out[3] = static_cast<std::uint8_t>(alpha);
      }
    }
    readback->Unmap();
    context->SetTarget(nullptr);
    return pixels;
  }

  std::vector<std::uint8_t> EncodePng(const std::vector<std::uint8_t>& pixels, UINT width, UINT height) {
    ComPtr<IStream> memory;
    CHECK_HR(CreateStreamOnHGlobal(nullptr, TRUE, &memory));
    ComPtr<IWICBitmapEncoder> encoder;
    CHECK_HR(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
    CHECK_HR(encoder->Initialize(memory.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    CHECK_HR(encoder->CreateNewFrame(&frame, nullptr));
    CHECK_HR(frame->Initialize(nullptr));
    CHECK_HR(frame->SetSize(width, height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    CHECK_HR(frame->SetPixelFormat(&format));
    CHECK_HR(frame->WritePixels(height, width * 4, static_cast<UINT>(pixels.size()),
                                const_cast<BYTE*>(pixels.data())));
    CHECK_HR(frame->Commit());
    CHECK_HR(encoder->Commit());
    STATSTG stat{};
    CHECK_HR(memory->Stat(&stat, STATFLAG_NONAME));
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stat.cbSize.QuadPart));
    LARGE_INTEGER zero{};
    CHECK_HR(memory->Seek(zero, STREAM_SEEK_SET, nullptr));
    ULONG read = 0;
    CHECK_HR(memory->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read));
    return bytes;
  }
};

bool WriteFile(const std::wstring& path, const std::vector<std::uint8_t>& bytes) {
  FILE* file = _wfopen(path.c_str(), L"wb");
  if (file == nullptr) return false;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  return std::fclose(file) == 0 && ok;
}

std::vector<std::uint8_t> BuildIco(const std::vector<std::pair<UINT, std::vector<std::uint8_t>>>& images) {
  std::vector<std::uint8_t> out;
  auto put16 = [&](std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
  };
  auto put32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  };
  put16(0);
  put16(1);
  put16(static_cast<std::uint16_t>(images.size()));
  std::uint32_t offset = 6 + 16 * static_cast<std::uint32_t>(images.size());
  for (const auto& [size, png] : images) {
    out.push_back(static_cast<std::uint8_t>(size >= 256 ? 0 : size));
    out.push_back(static_cast<std::uint8_t>(size >= 256 ? 0 : size));
    out.push_back(0);
    out.push_back(0);
    put16(1);
    put16(32);
    put32(static_cast<std::uint32_t>(png.size()));
    put32(offset);
    offset += static_cast<std::uint32_t>(png.size());
  }
  for (const auto& [size, png] : images) out.insert(out.end(), png.begin(), png.end());
  return out;
}

}

int wmain(int argc, wchar_t** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "usage: svg_rasterize png <in.svg> <out.png> <width> [height]\n"
                         "       svg_rasterize ico <in.svg> <out.ico> <size>[,<size>...]\n");
    return 1;
  }
  CHECK_HR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
  Renderer renderer;
  const std::wstring mode = argv[1];
  const std::wstring input = argv[2];
  const std::wstring output = argv[3];
  if (mode == L"png") {
    const UINT width = static_cast<UINT>(_wtoi(argv[4]));
    const UINT height = argc > 5 ? static_cast<UINT>(_wtoi(argv[5])) : width;
    const auto pixels = renderer.Render(input, width, height);
    return WriteFile(output, renderer.EncodePng(pixels, width, height)) ? 0 : 3;
  }
  if (mode == L"ico") {
    std::vector<std::pair<UINT, std::vector<std::uint8_t>>> images;
    std::wstring sizes = argv[4];
    std::size_t start = 0;
    while (start < sizes.size()) {
      std::size_t comma = sizes.find(L',', start);
      if (comma == std::wstring::npos) comma = sizes.size();
      const UINT size = static_cast<UINT>(std::stoul(sizes.substr(start, comma - start)));
      images.emplace_back(size, renderer.EncodePng(renderer.Render(input, size, size), size, size));
      start = comma + 1;
    }
    return WriteFile(output, BuildIco(images)) ? 0 : 3;
  }
  std::fprintf(stderr, "svg_rasterize: unknown mode\n");
  return 1;
}
