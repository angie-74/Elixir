// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/audio_devices.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include "core/util.h"

namespace elixir::launcher {
namespace {

std::vector<AudioDevice> EnumerateAudioEndpoints(EDataFlow direction, std::wstring* error) {
  using Microsoft::WRL::ComPtr;
  std::vector<AudioDevice> devices;
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    if (error) *error = L"Your audio devices couldn’t be listed.";
    return devices;
  }
  std::wstring default_id;
  ComPtr<IMMDevice> default_device;
  if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(direction, eConsole, &default_device))) {
    LPWSTR id = nullptr;
    if (SUCCEEDED(default_device->GetId(&id)) && id != nullptr) default_id = id;
    CoTaskMemFree(id);
  }
  ComPtr<IMMDeviceCollection> collection;
  hr = enumerator->EnumAudioEndpoints(direction, DEVICE_STATE_ACTIVE, &collection);
  if (FAILED(hr)) {
    if (error) *error = L"Your audio devices couldn’t be listed.";
    return devices;
  }
  UINT count = 0;
  if (FAILED(collection->GetCount(&count))) {
    if (error) *error = L"Your audio devices couldn’t be listed.";
    return devices;
  }
  for (UINT i = 0; i < count; ++i) {
    ComPtr<IMMDevice> device;
    if (FAILED(collection->Item(i, &device))) continue;
    AudioDevice entry;
    LPWSTR id = nullptr;
    if (SUCCEEDED(device->GetId(&id)) && id != nullptr) entry.id = id;
    CoTaskMemFree(id);
    if (entry.id.empty()) continue;
    ComPtr<IPropertyStore> store;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
      PROPVARIANT value;
      PropVariantInit(&value);
      if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR &&
          value.pwszVal != nullptr) {
        entry.name = value.pwszVal;
      }
      PropVariantClear(&value);
    }
    if (entry.name.empty()) entry.name = entry.id;
    entry.is_default = !default_id.empty() && entry.id == default_id;
    devices.push_back(std::move(entry));
  }
  return devices;
}

}

std::vector<AudioDevice> EnumerateAudioOutputs(std::wstring* error) { return EnumerateAudioEndpoints(eRender, error); }

std::vector<AudioDevice> EnumerateAudioInputs(std::wstring* error) { return EnumerateAudioEndpoints(eCapture, error); }

}
