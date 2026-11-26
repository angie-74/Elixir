// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "audio/wasapi_audio.h"

#include <windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "platform/windows/guest_tls.h"

namespace mocktail::audio {
namespace {

using Microsoft::WRL::ComPtr;

constexpr DWORD kAudioThreadStackReserve = 1024 * 1024 + 64 * 1024;
constexpr ULONG kStackGuarantee = 32 * 1024;
constexpr DWORD kCommandTimeoutMs = 5000;
constexpr int kNullRate = 48000;
constexpr int kNullChannels = 2;
constexpr std::uint32_t kNullPeriodFrames = 480;
constexpr std::uint32_t kZombieRetryTicks = 50;
constexpr REFERENCE_TIME kCaptureBufferDuration = 400000;

constexpr GUID kSubtypePcm = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr GUID kSubtypeFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

std::string HresultText(HRESULT hr) {
  const char* name = nullptr;
  switch (hr) {
    case AUDCLNT_E_DEVICE_INVALIDATED: name = "AUDCLNT_E_DEVICE_INVALIDATED"; break;
    case AUDCLNT_E_UNSUPPORTED_FORMAT: name = "AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
    case AUDCLNT_E_ENGINE_PERIODICITY_LOCKED: name = "AUDCLNT_E_ENGINE_PERIODICITY_LOCKED"; break;
    case AUDCLNT_E_DEVICE_IN_USE: name = "AUDCLNT_E_DEVICE_IN_USE"; break;
    case AUDCLNT_E_SERVICE_NOT_RUNNING: name = "AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
    case AUDCLNT_E_BUFFER_SIZE_ERROR: name = "AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
    case E_ACCESSDENIED: name = "E_ACCESSDENIED"; break;
    case E_NOTFOUND: name = "E_NOTFOUND"; break;
    case E_OUTOFMEMORY: name = "E_OUTOFMEMORY"; break;
    case E_NOINTERFACE: name = "E_NOINTERFACE"; break;
    default: break;
  }
  char text[96];
  std::snprintf(text, sizeof(text), "0x%08lx%s%s%s", static_cast<unsigned long>(hr),
                name != nullptr ? " (" : "", name != nullptr ? name : "", name != nullptr ? ")" : "");
  return text;
}

Status HresultStatus(const char* operation, HRESULT hr) {
  std::string message(operation);
  message += " failed: ";
  message += HresultText(hr);
  const StatusCode code = (hr == E_NOTFOUND || hr == AUDCLNT_E_DEVICE_INVALIDATED ||
                           hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
                              ? StatusCode::kUnavailable
                              : StatusCode::kPlatformError;
  return Status::Error(code, std::move(message));
}

Status InvalidArgument(std::string message) {
  return Status::Error(StatusCode::kInvalidArgument, std::move(message));
}

Status FailedPrecondition(std::string message) {
  return Status::Error(StatusCode::kFailedPrecondition, std::move(message));
}

void LogLine(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  std::fprintf(stderr, "[elixir][audio] %s\n", line);
}

void RunInMta(const std::function<void()>& work) {
  APTTYPE type{};
  APTTYPEQUALIFIER qualifier{};
  const HRESULT hr = CoGetApartmentType(&type, &qualifier);
  if (SUCCEEDED(hr) && type == APTTYPE_MTA) {
    work();
    return;
  }
  if (hr == CO_E_NOTINITIALIZED) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(init)) {
      work();
      CoUninitialize();
      return;
    }
  }
  std::thread helper([&work] {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    work();
    if (SUCCEEDED(init)) CoUninitialize();
  });
  helper.join();
}

bool ParseMixFormat(const WAVEFORMATEX* format, PcmLayout* layout, std::uint32_t* valid_bits) {
  if (format == nullptr || format->nChannels == 0 || format->nChannels > 8) return false;
  layout->sample_rate_hz = static_cast<int>(format->nSamplesPerSec);
  layout->channels = format->nChannels;
  layout->channel_mask = 0;
  bool is_float = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
  bool is_pcm = format->wFormatTag == WAVE_FORMAT_PCM;
  std::uint32_t valid = format->wBitsPerSample;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    is_float = IsEqualGUID(extensible->SubFormat, kSubtypeFloat) != 0;
    is_pcm = IsEqualGUID(extensible->SubFormat, kSubtypePcm) != 0;
    layout->channel_mask = extensible->dwChannelMask;
    if (extensible->Samples.wValidBitsPerSample != 0) valid = extensible->Samples.wValidBitsPerSample;
  }
  *valid_bits = valid;
  if (is_float && format->wBitsPerSample == 32) {
    layout->type = PcmSampleType::kF32;
    return true;
  }
  if (!is_pcm) return false;
  switch (format->wBitsPerSample) {
    case 8: layout->type = PcmSampleType::kU8; return true;
    case 16: layout->type = PcmSampleType::kS16; return true;
    case 24: layout->type = PcmSampleType::kS24Packed; return true;
    case 32: layout->type = PcmSampleType::kS32; return true;
    default: return false;
  }
}

std::string FriendlyName(IMMDevice* device) {
  ComPtr<IPropertyStore> store;
  std::string name;
  if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
      name = WideToUtf8(value.pwszVal);
    }
    PropVariantClear(&value);
  }
  return name;
}

std::wstring DeviceId(IMMDevice* device) {
  LPWSTR id = nullptr;
  std::wstring result;
  if (SUCCEEDED(device->GetId(&id)) && id != nullptr) result = id;
  CoTaskMemFree(id);
  return result;
}

Status ListDevices(EDataFlow flow, std::vector<WasapiAudioDevice>* devices) {
  Status status = Status::Ok();
  devices->clear();
  RunInMta([&] {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
      status = HresultStatus("CoCreateInstance(MMDeviceEnumerator)", hr);
      return;
    }
    std::wstring default_id;
    ComPtr<IMMDevice> default_device;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &default_device))) {
      default_id = DeviceId(default_device.Get());
    }
    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
      status = HresultStatus("IMMDeviceEnumerator::EnumAudioEndpoints", hr);
      return;
    }
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT index = 0; index < count; ++index) {
      ComPtr<IMMDevice> device;
      if (FAILED(collection->Item(index, &device))) continue;
      WasapiAudioDevice entry;
      entry.id = DeviceId(device.Get());
      entry.name = FriendlyName(device.Get());
      if (entry.name.empty()) entry.name = WideToUtf8(entry.id);
      entry.is_default = !default_id.empty() && entry.id == default_id;
      devices->push_back(std::move(entry));
    }
  });
  return status;
}

double FramesToMs(double frames, int rate) {
  return rate > 0 ? frames * 1000.0 / rate : 0.0;
}

double HnsToMs(REFERENCE_TIME hns) { return static_cast<double>(hns) / 10000.0; }

HANDLE CreateGuestCapableThread(LPTHREAD_START_ROUTINE start, void* parameter, DWORD* thread_id) {
  return CreateThread(nullptr, kAudioThreadStackReserve, start, parameter, STACK_SIZE_PARAM_IS_A_RESERVATION,
                      thread_id);
}

class MmcssRegistration final {
 public:
  explicit MmcssRegistration(bool enabled) {
    if (!enabled) return;
    DWORD task_index = 0;
    handle_ = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
  }
  ~MmcssRegistration() {
    if (handle_ != nullptr) AvRevertMmThreadCharacteristics(handle_);
  }
  MmcssRegistration(const MmcssRegistration&) = delete;
  MmcssRegistration& operator=(const MmcssRegistration&) = delete;
  bool registered() const { return handle_ != nullptr; }

 private:
  HANDLE handle_ = nullptr;
};

class FrameClock final {
 public:
  FrameClock() {
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                    TIMER_ALL_ACCESS);
    if (timer_ == nullptr) timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    QueryPerformanceFrequency(&frequency_);
  }
  ~FrameClock() {
    if (timer_ != nullptr) CloseHandle(timer_);
  }
  FrameClock(const FrameClock&) = delete;
  FrameClock& operator=(const FrameClock&) = delete;
  HANDLE handle() const { return timer_; }
  void Start(int rate) {
    rate_ = rate;
    QueryPerformanceCounter(&start_);
    emitted_ = 0;
    LARGE_INTEGER due;
    due.QuadPart = -100000;
    SetWaitableTimer(timer_, &due, 10, nullptr, nullptr, FALSE);
    running_ = true;
  }
  void Stop() {
    if (running_) CancelWaitableTimer(timer_);
    running_ = false;
  }
  bool running() const { return running_; }
  std::uint64_t Due() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double seconds = static_cast<double>(now.QuadPart - start_.QuadPart) /
                           static_cast<double>(frequency_.QuadPart);
    const auto total = static_cast<std::uint64_t>(seconds * rate_);
    const std::uint64_t due = total > emitted_ ? total - emitted_ : 0;
    emitted_ = total;
    return due;
  }

 private:
  HANDLE timer_ = nullptr;
  LARGE_INTEGER frequency_{};
  LARGE_INTEGER start_{};
  std::uint64_t emitted_ = 0;
  int rate_ = kNullRate;
  bool running_ = false;
};

struct ResolvedEndpoint {
  ComPtr<IMMDevice> device;
  std::wstring id;
  std::string name;
};

Status ResolveEndpoint(IMMDeviceEnumerator* enumerator, EDataFlow flow, bool follow_default,
                       const std::wstring& explicit_id, ResolvedEndpoint* resolved) {
  HRESULT hr;
  if (follow_default) {
    hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &resolved->device);
    if (FAILED(hr)) return HresultStatus("IMMDeviceEnumerator::GetDefaultAudioEndpoint", hr);
  } else {
    hr = enumerator->GetDevice(explicit_id.c_str(), &resolved->device);
    if (FAILED(hr)) return HresultStatus("IMMDeviceEnumerator::GetDevice", hr);
    DWORD state = 0;
    if (FAILED(resolved->device->GetState(&state)) || state != DEVICE_STATE_ACTIVE) {
      resolved->device.Reset();
      return Status::Error(StatusCode::kUnavailable, "the selected audio endpoint is not active");
    }
  }
  resolved->id = DeviceId(resolved->device.Get());
  resolved->name = FriendlyName(resolved->device.Get());
  return Status::Ok();
}

struct SinkState;
struct CaptureState;

class NotificationClient;

struct Subsystem {
  std::mutex mutex;
  bool initialized = false;
  WasapiAudioSubsystemOptions options;
  bool mta_usage = false;
  CO_MTA_USAGE_COOKIE mta_cookie{};
  ComPtr<IMMDeviceEnumerator> enumerator;
  NotificationClient* notifier = nullptr;
  std::wstring playback_id;
  std::string playback_name = "default";
  std::wstring recording_id;
  std::string recording_name = "default";
  std::vector<std::shared_ptr<SinkState>> sinks;
  std::vector<std::shared_ptr<CaptureState>> captures;
};

Subsystem& State() {
  static Subsystem* subsystem = new Subsystem();
  return *subsystem;
}

std::atomic<bool> g_microphone_permitted{false};

void OnEndpointStateChanged(std::wstring_view id);

class NotificationClient final : public IMMNotificationClient {
 public:
  ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG remaining = --references_;
    if (remaining == 0) delete this;
    return remaining;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
    if (object == nullptr) return E_POINTER;
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
      *object = static_cast<IMMNotificationClient*>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override {
    if (role == eConsole && (flow == eRender || flow == eCapture)) {
      NotifyWasapiDefaultDeviceChanged(flow == eRender, id != nullptr ? id : L"", false);
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override {
    if (id != nullptr) OnEndpointStateChanged(id);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override {
    if (id != nullptr) OnEndpointStateChanged(id);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD) override {
    if (id != nullptr) OnEndpointStateChanged(id);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

 private:
  std::atomic<ULONG> references_{1};
};

struct OutputDevice {
  ComPtr<IMMDevice> device;
  ComPtr<IAudioClient> client;
  ComPtr<IAudioRenderClient> render;
  ComPtr<IAudioClock> clock;
  HANDLE event = nullptr;
  std::wstring id;
  std::string name;
  PcmLayout layout;
  std::uint32_t valid_bits = 0;
  UINT32 buffer_frames = 0;
  UINT64 clock_frequency = 0;
  std::uint64_t written_frames = 0;
  bool started = false;
  bool low_latency = false;
  std::uint32_t engine_default = 0, engine_fundamental = 0, engine_min = 0, engine_max = 0;
  std::uint32_t stream_period = 0;
  double default_period_ms = 0, min_period_ms = 0, latency_ms = 0;

  OutputDevice() = default;
  OutputDevice(const OutputDevice&) = delete;
  OutputDevice& operator=(const OutputDevice&) = delete;
  ~OutputDevice() { Close(); }

  bool open() const { return client != nullptr; }
  void Close() {
    if (client != nullptr && started) client->Stop();
    started = false;
    clock.Reset();
    render.Reset();
    client.Reset();
    device.Reset();
    if (event != nullptr) {
      CloseHandle(event);
      event = nullptr;
    }
  }
  void Swap(OutputDevice& other) {
    std::swap(device, other.device);
    std::swap(client, other.client);
    std::swap(render, other.render);
    std::swap(clock, other.clock);
    std::swap(event, other.event);
    std::swap(id, other.id);
    std::swap(name, other.name);
    std::swap(layout, other.layout);
    std::swap(valid_bits, other.valid_bits);
    std::swap(buffer_frames, other.buffer_frames);
    std::swap(clock_frequency, other.clock_frequency);
    std::swap(written_frames, other.written_frames);
    std::swap(started, other.started);
    std::swap(low_latency, other.low_latency);
    std::swap(engine_default, other.engine_default);
    std::swap(engine_fundamental, other.engine_fundamental);
    std::swap(engine_min, other.engine_min);
    std::swap(engine_max, other.engine_max);
    std::swap(stream_period, other.stream_period);
    std::swap(default_period_ms, other.default_period_ms);
    std::swap(min_period_ms, other.min_period_ms);
    std::swap(latency_ms, other.latency_ms);
  }
};

Status OpenOutputDevice(IMMDeviceEnumerator* enumerator, bool follow_default, const std::wstring& id,
                        bool low_latency, OutputDevice* out) {
  ResolvedEndpoint endpoint;
  Status status = ResolveEndpoint(enumerator, eRender, follow_default, id, &endpoint);
  if (!status.ok()) return status;
  OutputDevice device;
  device.device = endpoint.device;
  device.id = endpoint.id;
  device.name = endpoint.name;

  WAVEFORMATEX* mix = nullptr;
  ComPtr<IAudioClient3> client3;
  HRESULT hr = device.device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(client3.GetAddressOf()));
  if (SUCCEEDED(hr)) {
    device.client = client3;
  } else {
    hr = device.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(device.client.GetAddressOf()));
    if (FAILED(hr)) return HresultStatus("IMMDevice::Activate(IAudioClient)", hr);
  }
  hr = device.client->GetMixFormat(&mix);
  if (FAILED(hr) || mix == nullptr) return HresultStatus("IAudioClient::GetMixFormat", hr);
  struct MixFree {
    WAVEFORMATEX* format;
    ~MixFree() { CoTaskMemFree(format); }
  } mix_free{mix};
  if (!ParseMixFormat(mix, &device.layout, &device.valid_bits)) {
    return Status::Error(StatusCode::kUnsupported, "the endpoint's mix format is not PCM or float");
  }

  bool initialized = false;
  if (low_latency && client3 != nullptr) {
    UINT32 default_period = 0, fundamental = 0, minimum = 0, maximum = 0;
    hr = client3->GetSharedModeEnginePeriod(mix, &default_period, &fundamental, &minimum, &maximum);
    if (SUCCEEDED(hr)) {
      device.engine_default = default_period;
      device.engine_fundamental = fundamental;
      device.engine_min = minimum;
      device.engine_max = maximum;
      hr = client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimum, mix, nullptr);
      UINT32 period = minimum;
      if (hr == AUDCLNT_E_ENGINE_PERIODICITY_LOCKED) {
        WAVEFORMATEX* current_format = nullptr;
        UINT32 current = 0;
        if (SUCCEEDED(client3->GetCurrentSharedModeEnginePeriod(&current_format, &current))) {
          period = current;
          hr = client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, current, mix,
                                                    nullptr);
        }
        CoTaskMemFree(current_format);
      }
      if (SUCCEEDED(hr)) {
        initialized = true;
        device.low_latency = true;
        device.stream_period = period;
      }
    }
    if (!initialized) {
      client3.Reset();
      device.client.Reset();
      hr = device.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(device.client.GetAddressOf()));
      if (FAILED(hr)) return HresultStatus("IMMDevice::Activate(IAudioClient)", hr);
    }
  }
  if (!initialized) {
    hr = device.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, mix, nullptr);
    if (FAILED(hr)) return HresultStatus("IAudioClient::Initialize", hr);
  }
  REFERENCE_TIME default_period = 0, minimum_period = 0, latency = 0;
  if (SUCCEEDED(device.client->GetDevicePeriod(&default_period, &minimum_period))) {
    device.default_period_ms = HnsToMs(default_period);
    device.min_period_ms = HnsToMs(minimum_period);
    if (!device.low_latency) {
      device.stream_period = static_cast<std::uint32_t>(
          (default_period * device.layout.sample_rate_hz + 5000000) / 10000000);
    }
  }
  device.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (device.event == nullptr) return Status::Error(StatusCode::kPlatformError, "CreateEvent failed");
  hr = device.client->SetEventHandle(device.event);
  if (FAILED(hr)) return HresultStatus("IAudioClient::SetEventHandle", hr);
  hr = device.client->GetBufferSize(&device.buffer_frames);
  if (FAILED(hr)) return HresultStatus("IAudioClient::GetBufferSize", hr);
  hr = device.client->GetService(IID_PPV_ARGS(&device.render));
  if (FAILED(hr)) return HresultStatus("IAudioClient::GetService(IAudioRenderClient)", hr);
  if (SUCCEEDED(device.client->GetService(IID_PPV_ARGS(&device.clock)))) {
    device.clock->GetFrequency(&device.clock_frequency);
  }
  if (SUCCEEDED(device.client->GetStreamLatency(&latency))) device.latency_ms = HnsToMs(latency);
  out->Swap(device);
  return Status::Ok();
}

struct SinkState {
  explicit SinkState(const PcmSpec& spec) : queue(spec) {}
  ~SinkState() {
    if (command != nullptr) CloseHandle(command);
  }
  SinkState(const SinkState&) = delete;
  SinkState& operator=(const SinkState&) = delete;

  PcmRenderQueue queue;
  std::mutex mutex;
  std::condition_variable cv;
  bool follow_default = true;
  std::wstring route_id;
  bool stop = false;
  bool want_running = false;
  bool reevaluate = false;
  bool force_reopen = false;
  std::uint64_t switch_request = 0;
  std::uint64_t switch_done = 0;
  bool switch_follow_default = true;
  std::wstring switch_id;
  Status switch_status;
  HANDLE command = nullptr;
  HANDLE thread = nullptr;
  DWORD thread_id = 0;
  bool init_done = false;
  Status init_status;
  bool exited = false;
  AudioOutputDriver driver = AudioOutputDriver::kWasapi;
  bool mmcss = true;
  bool low_latency = true;
  bool enters_guest = false;
  WasapiStreamInfo info;
  double queue_ms_sum = 0;
  std::uint64_t queue_ms_samples = 0;

  void Signal() { SetEvent(command); }
};

void PublishDevice(SinkState& state, const OutputDevice& device, bool zombie) {
  std::lock_guard<std::mutex> lock(state.mutex);
  WasapiStreamInfo& info = state.info;
  info.driver = state.driver == AudioOutputDriver::kNull ? "null" : zombie ? "zombie" : "wasapi";
  info.endpoint_id = zombie ? std::wstring() : device.id;
  info.endpoint_name = zombie ? std::string() : device.name;
  info.follows_default = state.follow_default;
  if (!zombie) {
    info.device_layout = device.layout;
    info.device_valid_bits = device.valid_bits;
    info.low_latency_stream = device.low_latency;
    info.engine_default_period_frames = device.engine_default;
    info.engine_fundamental_period_frames = device.engine_fundamental;
    info.engine_min_period_frames = device.engine_min;
    info.engine_max_period_frames = device.engine_max;
    info.stream_period_frames = device.stream_period;
    info.device_default_period_ms = device.default_period_ms;
    info.device_minimum_period_ms = device.min_period_ms;
    info.buffer_frames = device.buffer_frames;
    info.stream_latency_ms = device.latency_ms;
  }
}

class SinkThread final {
 public:
  explicit SinkThread(std::shared_ptr<SinkState> state) : state_(std::move(state)) {}

  void Run() {
    SinkState& s = *state_;
    ULONG guarantee = kStackGuarantee;
    SetThreadStackGuarantee(&guarantee);
    SetThreadDescription(GetCurrentThread(), L"Elixir audio render");
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (s.enters_guest) elixir::platform::EnsureGuestThread();
    MmcssRegistration mmcss(s.mmcss);
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.info.mmcss_registered = mmcss.registered();
    }

    Status status = Status::Ok();
    if (s.driver == AudioOutputDriver::kNull) {
      status = EnterClockMode( false);
    } else {
      const HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          IID_PPV_ARGS(&enumerator_));
      status = FAILED(hr) ? HresultStatus("CoCreateInstance(MMDeviceEnumerator)", hr)
                          : OpenRoute(s.follow_default, s.route_id);
    }
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.init_status = status;
      s.init_done = true;
    }
    s.cv.notify_all();
    if (status.ok()) Loop();

    device_.Close();
    clock_.Stop();
    enumerator_.Reset();
    if (SUCCEEDED(com)) CoUninitialize();
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.exited = true;
    }
    s.cv.notify_all();
  }

 private:
  Status EnterClockMode(bool zombie) {
    PcmLayout layout;
    layout.sample_rate_hz = kNullRate;
    layout.channels = kNullChannels;
    layout.type = PcmSampleType::kF32;
    if (zombie && clock_layout_valid_) layout = clock_layout_;
    const Status status = state_->queue.SetOutputLayout(layout);
    if (!status.ok()) return status;
    clock_layout_ = layout;
    clock_layout_valid_ = true;
    zombie_ = zombie;
    clock_mode_ = true;
    scratch_.resize(static_cast<std::size_t>(kNullPeriodFrames) * 4 * BytesPerFrame(layout));
    if (zombie) {
      OutputDevice none;
      PublishDevice(*state_, none, true);
    } else {
      std::lock_guard<std::mutex> lock(state_->mutex);
      WasapiStreamInfo& info = state_->info;
      info.driver = "null";
      info.device_layout = layout;
      info.stream_period_frames = kNullPeriodFrames;
      info.buffer_frames = kNullPeriodFrames;
    }
    UpdateClock();
    return Status::Ok();
  }

  void UpdateClock() {
    bool running;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      running = state_->want_running;
    }
    if (clock_mode_ && running && !clock_.running()) clock_.Start(clock_layout_.sample_rate_hz);
    if ((!clock_mode_ || !running) && clock_.running()) clock_.Stop();
  }

  Status OpenRoute(bool follow_default, const std::wstring& id) {
    OutputDevice opened;
    Status status = OpenOutputDevice(enumerator_.Get(), follow_default, id, state_->low_latency, &opened);
    if (!status.ok()) return status;
    status = state_->queue.SetOutputLayout(opened.layout);
    if (!status.ok()) return status;
    if (device_.open()) device_.Close();
    device_.Swap(opened);
    clock_mode_ = false;
    zombie_ = false;
    clock_.Stop();
    clock_layout_ = device_.layout;
    clock_layout_valid_ = true;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      if (state_->info.opens > 0) ++state_->info.reopens;
      ++state_->info.opens;
    }
    PublishDevice(*state_, device_, false);
    RenderDevice();
    return ApplyRunning();
  }

  Status ApplyRunning() {
    bool running;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      running = state_->want_running;
    }
    if (clock_mode_) {
      UpdateClock();
      return Status::Ok();
    }
    if (!device_.open()) return Status::Ok();
    if (running && !device_.started) {
      const HRESULT hr = device_.client->Start();
      if (FAILED(hr)) return HandleFailure("IAudioClient::Start", hr);
      device_.started = true;
    } else if (!running && device_.started) {
      device_.client->Stop();
      device_.started = false;
    }
    return Status::Ok();
  }

  Status HandleFailure(const char* operation, HRESULT hr) {
    const Status status = HresultStatus(operation, hr);
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      ++state_->info.device_invalidations;
    }
    LogLine("render stream lost its endpoint (%s); consuming at real time until one is available",
            status.message().c_str());
    device_.Close();
    (void)EnterClockMode( true);
    zombie_ticks_ = 0;
    return status;
  }

  void RenderDevice() {
    if (!device_.open()) return;
    UINT32 padding = 0;
    HRESULT hr = device_.client->GetCurrentPadding(&padding);
    if (FAILED(hr)) {
      (void)HandleFailure("IAudioClient::GetCurrentPadding", hr);
      return;
    }
    const UINT32 available = device_.buffer_frames > padding ? device_.buffer_frames - padding : 0;
    if (available == 0) return;
    BYTE* data = nullptr;
    hr = device_.render->GetBuffer(available, &data);
    if (FAILED(hr)) {
      (void)HandleFailure("IAudioRenderClient::GetBuffer", hr);
      return;
    }
    const std::size_t produced = state_->queue.Render(data, available);
    hr = device_.render->ReleaseBuffer(available, produced == 0 ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
    if (FAILED(hr)) {
      (void)HandleFailure("IAudioRenderClient::ReleaseBuffer", hr);
      return;
    }
    device_.written_frames += available;
    double queue_ms = FramesToMs(static_cast<double>(padding + available), device_.layout.sample_rate_hz);
    if (device_.clock != nullptr && device_.clock_frequency != 0 && device_.started) {
      UINT64 position = 0, qpc = 0;
      if (SUCCEEDED(device_.clock->GetPosition(&position, &qpc))) {
        const double played = static_cast<double>(position) * device_.layout.sample_rate_hz /
                              static_cast<double>(device_.clock_frequency);
        const double queued = static_cast<double>(device_.written_frames) - played;
        if (queued >= 0) queue_ms = FramesToMs(queued, device_.layout.sample_rate_hz);
      }
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    WasapiStreamInfo& info = state_->info;
    ++info.render_events;
    if (device_.started) {
      info.last_device_queue_ms = queue_ms;
      info.max_device_queue_ms = std::max(info.max_device_queue_ms, queue_ms);
      state_->queue_ms_sum += queue_ms;
      ++state_->queue_ms_samples;
      info.mean_device_queue_ms = state_->queue_ms_sum / static_cast<double>(state_->queue_ms_samples);
    }
  }

  void RenderClock() {
    const std::uint64_t due = clock_.Due();
    const std::size_t frame_bytes = BytesPerFrame(clock_layout_);
    const std::size_t capacity = scratch_.size() / frame_bytes;
    std::uint64_t remaining = std::min<std::uint64_t>(due, capacity * 8);
    while (remaining > 0) {
      const std::size_t frames = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, capacity));
      (void)state_->queue.Render(scratch_.data(), frames);
      remaining -= frames;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    ++state_->info.render_events;
  }

  void ProcessCommands(bool* stop) {
    SinkState& s = *state_;
    bool reevaluate = false;
    bool force = false;
    bool do_switch = false;
    bool switch_follow = true;
    std::wstring switch_id;
    std::uint64_t switch_request = 0;
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      *stop = s.stop;
      reevaluate = s.reevaluate;
      force = s.force_reopen;
      s.reevaluate = false;
      s.force_reopen = false;
      if (s.switch_request != s.switch_done) {
        do_switch = true;
        switch_follow = s.switch_follow_default;
        switch_id = s.switch_id;
        switch_request = s.switch_request;
      }
    }
    if (*stop) return;
    if (do_switch) {
      Status status = Status::Ok();
      if (s.driver != AudioOutputDriver::kNull) {
        status = OpenRoute(switch_follow, switch_id);
      }
      {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (status.ok()) {
          s.follow_default = switch_follow;
          s.route_id = switch_id;
          s.info.follows_default = switch_follow;
        }
        s.switch_status = status;
        s.switch_done = switch_request;
      }
      s.cv.notify_all();
    } else if (reevaluate && s.driver != AudioOutputDriver::kNull) {
      Reevaluate(force);
    }
    (void)ApplyRunning();
  }

  void Reevaluate(bool force) {
    SinkState& s = *state_;
    bool follow;
    std::wstring id;
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      follow = s.follow_default;
      id = s.route_id;
    }
    ResolvedEndpoint desired;
    bool use_default = follow;
    if (!follow && !ResolveEndpoint(enumerator_.Get(), eRender, false, id, &desired).ok()) {
      use_default = true;
    }
    if (use_default && !ResolveEndpoint(enumerator_.Get(), eRender, true, std::wstring(), &desired).ok()) {
      return;
    }
    if (!force && device_.open() && desired.id == device_.id) return;
    const Status status = OpenRoute(use_default, use_default ? std::wstring() : id);
    if (!status.ok()) {
      LogLine("cannot move render stream: %s", status.message().c_str());
      if (!device_.open() && !clock_mode_) (void)EnterClockMode(true);
    } else if (!follow && use_default) {
      LogLine("selected output endpoint is unavailable; playing on the system default");
    }
  }

  void Loop() {
    SinkState& s = *state_;
    for (;;) {
      HANDLE handles[2] = {s.command, nullptr};
      DWORD count = 1;
      if (device_.open() && device_.event != nullptr) {
        handles[count++] = device_.event;
      } else if (clock_mode_ && clock_.running()) {
        handles[count++] = clock_.handle();
      }
      const DWORD wait = WaitForMultipleObjects(count, handles, FALSE, clock_mode_ ? 100 : 2000);
      bool stop = false;
      if (wait == WAIT_OBJECT_0) {
        ProcessCommands(&stop);
        if (stop) return;
        continue;
      }
      if (wait == WAIT_OBJECT_0 + 1) {
        if (device_.open()) {
          RenderDevice();
        } else if (clock_mode_) {
          RenderClock();
        }
      }
      if (zombie_ && clock_mode_ && ++zombie_ticks_ >= kZombieRetryTicks) {
        zombie_ticks_ = 0;
        Reevaluate( false);
      }
    }
  }

  std::shared_ptr<SinkState> state_;
  ComPtr<IMMDeviceEnumerator> enumerator_;
  OutputDevice device_;
  FrameClock clock_;
  bool clock_mode_ = false;
  bool zombie_ = false;
  std::uint32_t zombie_ticks_ = 0;
  PcmLayout clock_layout_;
  bool clock_layout_valid_ = false;
  std::vector<std::uint8_t> scratch_;
};

DWORD WINAPI SinkThreadMain(void* parameter) {
  auto* holder = static_cast<std::shared_ptr<SinkState>*>(parameter);
  std::shared_ptr<SinkState> state = std::move(*holder);
  delete holder;
  SinkThread thread(state);
  thread.Run();
  return 0;
}

void RegisterSink(const std::shared_ptr<SinkState>& state) {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  subsystem.sinks.push_back(state);
}

void UnregisterSink(const SinkState* state) {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  auto& sinks = subsystem.sinks;
  sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
                             [state](const std::shared_ptr<SinkState>& entry) { return entry.get() == state; }),
              sinks.end());
}

class WasapiAudioSink final : public AudioSink {
 public:
  explicit WasapiAudioSink(std::shared_ptr<SinkState> state) : state_(std::move(state)) {}
  ~WasapiAudioSink() override { Shutdown(); }

  const PcmSpec& source_spec() const override { return state_->queue.source_spec(); }

  Status Enqueue(const PcmBuffer& buffer) override {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    return state_->queue.Enqueue(buffer);
  }

  Status Pause() override { return SetRunning(false); }
  Status Resume() override { return SetRunning(true); }

  Status SetGain(float linear_gain) override {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    return state_->queue.SetGain(linear_gain);
  }

  Status Flush() override {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    state_->queue.Flush();
    return Status::Ok();
  }

  Status Clear() override {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    state_->queue.Clear();
    return Status::Ok();
  }

  Status GetQueuedBytes(std::size_t* size_bytes) const override {
    if (size_bytes == nullptr) return InvalidArgument("queued-byte output pointer is null");
    *size_bytes = 0;
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    *size_bytes = state_->queue.QueuedBytes();
    return Status::Ok();
  }

  void Shutdown() override {
    std::unique_lock<std::mutex> shutdown_lock(shutdown_mutex_);
    if (shut_down_.exchange(true, std::memory_order_acq_rel)) return;
    SinkState& s = *state_;
    UnregisterSink(&s);
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.stop = true;
    }
    s.Signal();
    const bool on_render_thread = GetCurrentThreadId() == s.thread_id;
    if (!on_render_thread && s.thread != nullptr) {
      WaitForSingleObject(s.thread, INFINITE);
    }
    s.queue.ReleaseAll();
    if (s.thread != nullptr) {
      CloseHandle(s.thread);
      s.thread = nullptr;
    }
  }

  const std::shared_ptr<SinkState>& state() const { return state_; }

 private:
  Status SetRunning(bool running) {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio sink is shut down");
    state_->queue.SetPlaying(running);
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->want_running = running;
    }
    state_->Signal();
    return Status::Ok();
  }

  std::shared_ptr<SinkState> state_;
  std::mutex shutdown_mutex_;
  std::atomic<bool> shut_down_{false};
};

struct CaptureState {
  CaptureState() = default;
  ~CaptureState() {
    if (command != nullptr) CloseHandle(command);
  }
  CaptureState(const CaptureState&) = delete;
  CaptureState& operator=(const CaptureState&) = delete;

  std::mutex mutex;
  std::condition_variable cv;
  PcmSpec output_spec;
  std::size_t frames_per_buffer = 0;
  std::vector<std::uint8_t> buffer;
  AudioCaptureDataCallback callback = nullptr;
  void* callback_context = nullptr;
  bool follow_default = true;
  std::wstring route_id;
  bool stop = false;
  bool want_running = false;
  std::uint64_t run_request = 0;
  std::uint64_t run_done = 0;
  bool reevaluate = false;
  bool force_reopen = false;
  std::uint64_t switch_request = 0;
  std::uint64_t switch_done = 0;
  bool switch_follow_default = true;
  std::wstring switch_id;
  Status switch_status;
  HANDLE command = nullptr;
  HANDLE thread = nullptr;
  DWORD thread_id = 0;
  bool init_done = false;
  Status init_status;
  AudioOutputDriver driver = AudioOutputDriver::kWasapi;
  bool mmcss = true;
  bool enters_guest = false;
  WasapiCaptureInfo info;

  void Signal() { SetEvent(command); }
};

struct InputDevice {
  ComPtr<IMMDevice> device;
  ComPtr<IAudioClient> client;
  ComPtr<IAudioCaptureClient> capture;
  HANDLE event = nullptr;
  std::wstring id;
  PcmLayout layout;
  std::uint32_t valid_bits = 0;
  UINT32 buffer_frames = 0;
  double latency_ms = 0;
  bool started = false;

  InputDevice() = default;
  InputDevice(const InputDevice&) = delete;
  InputDevice& operator=(const InputDevice&) = delete;
  ~InputDevice() { Close(); }
  bool open() const { return client != nullptr; }
  void Close() {
    if (client != nullptr && started) client->Stop();
    started = false;
    capture.Reset();
    client.Reset();
    device.Reset();
    if (event != nullptr) {
      CloseHandle(event);
      event = nullptr;
    }
  }
  void Swap(InputDevice& other) {
    std::swap(device, other.device);
    std::swap(client, other.client);
    std::swap(capture, other.capture);
    std::swap(event, other.event);
    std::swap(id, other.id);
    std::swap(layout, other.layout);
    std::swap(valid_bits, other.valid_bits);
    std::swap(buffer_frames, other.buffer_frames);
    std::swap(latency_ms, other.latency_ms);
    std::swap(started, other.started);
  }
};

Status OpenInputDevice(IMMDeviceEnumerator* enumerator, bool follow_default, const std::wstring& id,
                       InputDevice* out) {
  ResolvedEndpoint endpoint;
  Status status = ResolveEndpoint(enumerator, eCapture, follow_default, id, &endpoint);
  if (!status.ok()) return status;
  InputDevice device;
  device.device = endpoint.device;
  device.id = endpoint.id;
  HRESULT hr = device.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(device.client.GetAddressOf()));
  if (FAILED(hr)) return HresultStatus("IMMDevice::Activate(IAudioClient, capture)", hr);
  WAVEFORMATEX* mix = nullptr;
  hr = device.client->GetMixFormat(&mix);
  if (FAILED(hr) || mix == nullptr) return HresultStatus("IAudioClient::GetMixFormat(capture)", hr);
  struct MixFree {
    WAVEFORMATEX* format;
    ~MixFree() { CoTaskMemFree(format); }
  } mix_free{mix};
  if (!ParseMixFormat(mix, &device.layout, &device.valid_bits)) {
    return Status::Error(StatusCode::kUnsupported, "the capture endpoint's mix format is not PCM or float");
  }
  hr = device.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, kCaptureBufferDuration, 0,
                                 mix, nullptr);
  if (hr == E_ACCESSDENIED) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "Windows privacy settings deny microphone access to this app (E_ACCESSDENIED)");
  }
  if (FAILED(hr)) return HresultStatus("IAudioClient::Initialize(capture)", hr);
  device.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (device.event == nullptr) return Status::Error(StatusCode::kPlatformError, "CreateEvent failed");
  hr = device.client->SetEventHandle(device.event);
  if (FAILED(hr)) return HresultStatus("IAudioClient::SetEventHandle(capture)", hr);
  hr = device.client->GetBufferSize(&device.buffer_frames);
  if (FAILED(hr)) return HresultStatus("IAudioClient::GetBufferSize(capture)", hr);
  hr = device.client->GetService(IID_PPV_ARGS(&device.capture));
  if (FAILED(hr)) return HresultStatus("IAudioClient::GetService(IAudioCaptureClient)", hr);
  REFERENCE_TIME latency = 0;
  if (SUCCEEDED(device.client->GetStreamLatency(&latency))) device.latency_ms = HnsToMs(latency);
  out->Swap(device);
  return Status::Ok();
}

class CaptureThread final {
 public:
  explicit CaptureThread(std::shared_ptr<CaptureState> state) : state_(std::move(state)) {}

  void Run() {
    CaptureState& s = *state_;
    ULONG guarantee = kStackGuarantee;
    SetThreadStackGuarantee(&guarantee);
    SetThreadDescription(GetCurrentThread(), L"Elixir audio capture");
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (s.enters_guest) elixir::platform::EnsureGuestThread();
    MmcssRegistration mmcss(s.mmcss);
    accumulator_.resize(s.buffer.size());
    Status status = Status::Ok();
    if (s.driver == AudioOutputDriver::kNull) {
      status = EnterClockMode(false);
    } else {
      const HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          IID_PPV_ARGS(&enumerator_));
      status = FAILED(hr) ? HresultStatus("CoCreateInstance(MMDeviceEnumerator)", hr)
                          : OpenRoute(s.follow_default, s.route_id);
    }
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.info.mmcss_registered = mmcss.registered();
      s.init_status = status;
      s.init_done = true;
    }
    s.cv.notify_all();
    if (status.ok()) Loop();
    device_.Close();
    clock_.Stop();
    enumerator_.Reset();
    if (SUCCEEDED(com)) CoUninitialize();
  }

 private:
  Status Configure(const PcmLayout& device_layout) {
    PcmLayout output = LayoutFromPcmSpec(state_->output_spec);
    const Status status = converter_.Configure(device_layout, output);
    fill_frames_ = 0;
    return status;
  }

  Status EnterClockMode(bool zombie) {
    PcmLayout layout;
    layout.sample_rate_hz = kNullRate;
    layout.channels = kNullChannels;
    layout.type = PcmSampleType::kF32;
    const Status status = Configure(layout);
    if (!status.ok()) return status;
    clock_layout_ = layout;
    clock_mode_ = true;
    zombie_ = zombie;
    silence_.assign(static_cast<std::size_t>(kNullPeriodFrames) * 4 * kNullChannels, 0.0F);
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->info.driver = state_->driver == AudioOutputDriver::kNull ? "null" : "zombie";
      state_->info.device_layout = layout;
    }
    return Status::Ok();
  }

  Status OpenRoute(bool follow_default, const std::wstring& id) {
    InputDevice opened;
    Status status = OpenInputDevice(enumerator_.Get(), follow_default, id, &opened);
    if (!status.ok()) return status;
    status = Configure(opened.layout);
    if (!status.ok()) return status;
    if (device_.open()) device_.Close();
    device_.Swap(opened);
    clock_mode_ = false;
    zombie_ = false;
    clock_.Stop();
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      WasapiCaptureInfo& info = state_->info;
      info.driver = "wasapi";
      info.endpoint_id = device_.id;
      info.device_layout = device_.layout;
      info.buffer_frames = device_.buffer_frames;
      info.stream_latency_ms = device_.latency_ms;
      ++info.opens;
    }
    return ApplyRunning();
  }

  Status ApplyRunning() {
    bool running;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      running = state_->want_running && g_microphone_permitted.load(std::memory_order_acquire);
    }
    if (clock_mode_) {
      if (running && !clock_.running()) clock_.Start(clock_layout_.sample_rate_hz);
      if (!running && clock_.running()) {
        clock_.Stop();
        converter_.Reset();
        fill_frames_ = 0;
      }
      return Status::Ok();
    }
    if (!device_.open()) return Status::Ok();
    if (running && !device_.started) {
      const HRESULT hr = device_.client->Start();
      if (FAILED(hr)) return HandleFailure(HresultStatus("IAudioClient::Start(capture)", hr));
      device_.started = true;
    } else if (!running && device_.started) {
      device_.client->Stop();
      device_.client->Reset();
      device_.started = false;
      converter_.Reset();
      fill_frames_ = 0;
    }
    return Status::Ok();
  }

  Status HandleFailure(Status status) {
    LogLine("capture stream lost its endpoint (%s); delivering silence until one is available",
            status.message().c_str());
    device_.Close();
    (void)EnterClockMode(true);
    (void)ApplyRunning();
    zombie_ticks_ = 0;
    return status;
  }

  void Deliver() {
    CaptureState& s = *state_;
    const std::size_t frame_bytes = BytesPerFrame(s.output_spec);
    for (;;) {
      const std::size_t produced = converter_.Pull(accumulator_.data() + fill_frames_ * frame_bytes,
                                                   s.frames_per_buffer - fill_frames_, 1.0F, false);
      fill_frames_ += produced;
      if (fill_frames_ < s.frames_per_buffer) return;
      fill_frames_ = 0;
      bool running;
      {
        std::lock_guard<std::mutex> lock(s.mutex);
        running = s.want_running && g_microphone_permitted.load(std::memory_order_acquire);
      }
      if (!running) return;
      std::memcpy(s.buffer.data(), accumulator_.data(), s.buffer.size());
      s.callback(s.callback_context, s.buffer.size());
      std::lock_guard<std::mutex> lock(s.mutex);
      ++s.info.delivered_buffers;
    }
  }

  void CaptureDevice() {
    for (;;) {
      UINT32 packet = 0;
      HRESULT hr = device_.capture->GetNextPacketSize(&packet);
      if (FAILED(hr)) {
        (void)HandleFailure(HresultStatus("IAudioCaptureClient::GetNextPacketSize", hr));
        return;
      }
      if (packet == 0) break;
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      hr = device_.capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
      if (FAILED(hr)) {
        (void)HandleFailure(HresultStatus("IAudioCaptureClient::GetBuffer", hr));
        return;
      }
      if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
        silence_bytes_.assign(static_cast<std::size_t>(frames) * BytesPerFrame(device_.layout),
                              device_.layout.type == PcmSampleType::kU8 ? 0x80 : 0);
        converter_.Push(silence_bytes_.data(), frames);
      } else {
        converter_.Push(data, frames);
      }
      device_.capture->ReleaseBuffer(frames);
      {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->info.captured_device_frames += frames;
        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0) ++state_->info.discontinuities;
      }
      Deliver();
    }
  }

  void CaptureClock() {
    std::uint64_t due = clock_.Due();
    const std::size_t capacity = silence_.size() / kNullChannels;
    while (due > 0) {
      const std::size_t frames = static_cast<std::size_t>(std::min<std::uint64_t>(due, capacity));
      converter_.Push(silence_.data(), frames);
      Deliver();
      due -= frames;
    }
  }

  void Reevaluate(bool force) {
    CaptureState& s = *state_;
    bool follow;
    std::wstring id;
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      follow = s.follow_default;
      id = s.route_id;
    }
    ResolvedEndpoint desired;
    const Status resolved = ResolveEndpoint(enumerator_.Get(), eCapture, follow, id, &desired);
    if (!resolved.ok()) {
      if (device_.open()) (void)HandleFailure(resolved);
      return;
    }
    if (!force && device_.open() && desired.id == device_.id) return;
    const Status status = OpenRoute(follow, id);
    if (!status.ok()) LogLine("cannot move capture stream: %s", status.message().c_str());
  }

  void ProcessCommands(bool* stop) {
    CaptureState& s = *state_;
    bool reevaluate, force, do_switch = false, switch_follow = true;
    std::wstring switch_id;
    std::uint64_t switch_request = 0, run_request = 0;
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      *stop = s.stop;
      reevaluate = s.reevaluate;
      force = s.force_reopen;
      s.reevaluate = false;
      s.force_reopen = false;
      run_request = s.run_request;
      if (s.switch_request != s.switch_done) {
        do_switch = true;
        switch_follow = s.switch_follow_default;
        switch_id = s.switch_id;
        switch_request = s.switch_request;
      }
    }
    if (*stop) return;
    if (do_switch) {
      Status status = Status::Ok();
      if (s.driver != AudioOutputDriver::kNull) status = OpenRoute(switch_follow, switch_id);
      {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (status.ok()) {
          s.follow_default = switch_follow;
          s.route_id = switch_id;
        }
        s.switch_status = status;
        s.switch_done = switch_request;
      }
      s.cv.notify_all();
    } else if (reevaluate && s.driver != AudioOutputDriver::kNull) {
      Reevaluate(force);
    }
    (void)ApplyRunning();
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.run_done = run_request;
    }
    s.cv.notify_all();
  }

  void Loop() {
    CaptureState& s = *state_;
    for (;;) {
      HANDLE handles[2] = {s.command, nullptr};
      DWORD count = 1;
      if (device_.open() && device_.event != nullptr) {
        handles[count++] = device_.event;
      } else if (clock_mode_ && clock_.running()) {
        handles[count++] = clock_.handle();
      }
      const DWORD wait = WaitForMultipleObjects(count, handles, FALSE, clock_mode_ ? 100 : 2000);
      if (wait == WAIT_OBJECT_0) {
        bool stop = false;
        ProcessCommands(&stop);
        if (stop) return;
        continue;
      }
      if (wait == WAIT_OBJECT_0 + 1) {
        if (device_.open()) {
          CaptureDevice();
        } else if (clock_mode_) {
          CaptureClock();
        }
      }
      if (zombie_ && clock_mode_ && ++zombie_ticks_ >= kZombieRetryTicks) {
        zombie_ticks_ = 0;
        Reevaluate(false);
      }
    }
  }

  std::shared_ptr<CaptureState> state_;
  ComPtr<IMMDeviceEnumerator> enumerator_;
  InputDevice device_;
  FrameClock clock_;
  PcmStreamConverter converter_;
  PcmLayout clock_layout_;
  bool clock_mode_ = false;
  bool zombie_ = false;
  std::uint32_t zombie_ticks_ = 0;
  std::vector<std::uint8_t> accumulator_;
  std::size_t fill_frames_ = 0;
  std::vector<float> silence_;
  std::vector<std::uint8_t> silence_bytes_;
};

DWORD WINAPI CaptureThreadMain(void* parameter) {
  auto* holder = static_cast<std::shared_ptr<CaptureState>*>(parameter);
  std::shared_ptr<CaptureState> state = std::move(*holder);
  delete holder;
  CaptureThread thread(state);
  thread.Run();
  return 0;
}

void WaitForRunState(CaptureState& s) {
  if (GetCurrentThreadId() == s.thread_id) return;
  std::unique_lock<std::mutex> lock(s.mutex);
  const std::uint64_t request = s.run_request;
  s.cv.wait_for(lock, std::chrono::milliseconds(kCommandTimeoutMs),
                [&s, request] { return s.run_done >= request || s.stop; });
}

class WasapiAudioCapture final : public AudioCapture {
 public:
  explicit WasapiAudioCapture(std::shared_ptr<CaptureState> state) : state_(std::move(state)) {}
  ~WasapiAudioCapture() override { Shutdown(); }

  const PcmSpec& output_spec() const override { return state_->output_spec; }
  void* buffer_data() override { return state_->buffer.data(); }
  std::size_t buffer_size_bytes() const override { return state_->buffer.size(); }
  std::size_t frames_per_buffer() const override { return state_->frames_per_buffer; }

  Status Start() override {
    if (shut_down_.load(std::memory_order_acquire)) return FailedPrecondition("audio capture is shut down");
    if (!MicrophoneCapturePermitted()) {
      return FailedPrecondition("microphone capture is not permitted by the runtime policy");
    }
    SetRunning(true);
    return Status::Ok();
  }

  Status Stop() override {
    if (shut_down_.load(std::memory_order_acquire)) return Status::Ok();
    SetRunning(false);
    return Status::Ok();
  }

  void Shutdown() override {
    std::lock_guard<std::mutex> shutdown_lock(shutdown_mutex_);
    if (shut_down_.exchange(true, std::memory_order_acq_rel)) return;
    CaptureState& s = *state_;
    {
      Subsystem& subsystem = State();
      std::lock_guard<std::mutex> lock(subsystem.mutex);
      auto& captures = subsystem.captures;
      captures.erase(std::remove_if(captures.begin(), captures.end(),
                                    [&s](const std::shared_ptr<CaptureState>& entry) { return entry.get() == &s; }),
                     captures.end());
    }
    {
      std::lock_guard<std::mutex> lock(s.mutex);
      s.stop = true;
      s.want_running = false;
    }
    s.Signal();
    if (GetCurrentThreadId() != s.thread_id && s.thread != nullptr) WaitForSingleObject(s.thread, INFINITE);
    if (s.thread != nullptr) {
      CloseHandle(s.thread);
      s.thread = nullptr;
    }
  }

  const std::shared_ptr<CaptureState>& state() const { return state_; }

 private:
  void SetRunning(bool running) {
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->want_running = running;
      ++state_->run_request;
    }
    state_->Signal();
    WaitForRunState(*state_);
  }

  std::shared_ptr<CaptureState> state_;
  std::mutex shutdown_mutex_;
  std::atomic<bool> shut_down_{false};
};

void OnEndpointStateChanged(std::wstring_view id) {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  for (const auto& sink : subsystem.sinks) {
    std::lock_guard<std::mutex> sink_lock(sink->mutex);
    if ((!sink->follow_default && sink->route_id == id) || sink->info.endpoint_id == id) {
      sink->reevaluate = true;
      sink->Signal();
    }
  }
  for (const auto& capture : subsystem.captures) {
    std::lock_guard<std::mutex> capture_lock(capture->mutex);
    if ((!capture->follow_default && capture->route_id == id) || capture->info.endpoint_id == id) {
      capture->reevaluate = true;
      capture->Signal();
    }
  }
}

Status ConfiguredRoute(bool render, bool* follow_default, std::wstring* id) {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (!subsystem.initialized) {
    return FailedPrecondition("the audio subsystem must be initialized by the process owner");
  }
  const std::wstring& configured = render ? subsystem.playback_id : subsystem.recording_id;
  *follow_default = configured.empty();
  *id = configured;
  return Status::Ok();
}

Status RouteForTarget(const AudioEndpointTarget& target, bool render, bool* follow_default, std::wstring* id) {
  switch (target.kind) {
    case AudioEndpointTarget::Kind::kConfigured:
      return ConfiguredRoute(render, follow_default, id);
    case AudioEndpointTarget::Kind::kSystemDefault:
      *follow_default = true;
      id->clear();
      return Status::Ok();
    case AudioEndpointTarget::Kind::kEndpoint:
      if (target.endpoint_id.empty()) return InvalidArgument("explicit audio endpoint ID is empty");
      *follow_default = false;
      *id = target.endpoint_id;
      return Status::Ok();
  }
  return InvalidArgument("unknown audio endpoint target");
}

Status ConfigureDevice(bool render, std::string_view requested, std::vector<WasapiAudioDevice>* available,
                       std::string* resolved_name, bool allow_device_name) {
  if (available == nullptr || resolved_name == nullptr) {
    return InvalidArgument("audio device configuration output pointer is null");
  }
  const std::string requested_copy(requested);
  Status status = render ? ListWasapiPlaybackDevices(available) : ListWasapiRecordingDevices(available);
  if (!status.ok() && requested_copy != "default") return status;
  if (!status.ok()) available->clear();
  std::wstring id;
  status = ResolveWasapiDevice(requested_copy, *available, &id, resolved_name, allow_device_name);
  if (!status.ok()) return status;
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (!subsystem.initialized) return FailedPrecondition("audio subsystem stopped during configuration");
  if (render ? !subsystem.sinks.empty() : !subsystem.captures.empty()) {
    return FailedPrecondition(render ? "the playback device must be configured before opening a sink"
                                     : "the recording device must be configured before opening a capture");
  }
  (render ? subsystem.playback_id : subsystem.recording_id) = id;
  (render ? subsystem.playback_name : subsystem.recording_name) = *resolved_name;
  return Status::Ok();
}

Status GetConfigured(bool render, std::wstring* id, std::string* name) {
  if (id == nullptr || name == nullptr) return InvalidArgument("configured audio device output pointer is null");
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (!subsystem.initialized) return FailedPrecondition("the audio subsystem is not initialized");
  *id = render ? subsystem.playback_id : subsystem.recording_id;
  *name = render ? subsystem.playback_name : subsystem.recording_name;
  return Status::Ok();
}

template <typename StatePtr>
Status RequestSwitch(const StatePtr& state, bool follow_default, const std::wstring& id, bool rollback = false) {
  std::unique_lock<std::mutex> lock(state->mutex);
  const bool previous_follow_default = state->follow_default;
  const std::wstring previous_id = state->route_id;
  const std::uint64_t request = ++state->switch_request;
  state->switch_follow_default = follow_default;
  state->switch_id = id;
  state->Signal();
  if (!state->cv.wait_for(lock, std::chrono::milliseconds(kCommandTimeoutMs),
                          [&] { return state->switch_done >= request || state->stop; })) {
    if (!rollback) {
      ++state->switch_request;
      state->switch_follow_default = previous_follow_default;
      state->switch_id = previous_id;
      state->Signal();
    }
    return Status::Error(StatusCode::kPlatformError, "audio stream did not answer the device switch");
  }
  if (state->stop) return FailedPrecondition("audio stream shut down during the device switch");
  return state->switch_status;
}

template <typename StatePtr>
void RouteOf(const StatePtr& state, bool* follow_default, std::wstring* id) {
  std::lock_guard<std::mutex> lock(state->mutex);
  *follow_default = state->follow_default;
  *id = state->route_id;
}

Status SwitchDevice(bool render, std::wstring_view endpoint_id, std::string* resolved_name) {
  if (resolved_name == nullptr) return InvalidArgument("audio device switch output pointer is null");
  resolved_name->clear();
  std::string target_name = "default";
  const std::wstring id(endpoint_id);
  if (!id.empty()) {
    std::vector<WasapiAudioDevice> devices;
    Status status = render ? ListWasapiPlaybackDevices(&devices) : ListWasapiRecordingDevices(&devices);
    if (!status.ok()) return status;
    const auto found = std::find_if(devices.begin(), devices.end(),
                                    [&id](const WasapiAudioDevice& device) { return device.id == id; });
    if (found == devices.end()) return Status::Error(StatusCode::kUnavailable, "selected audio endpoint is unavailable");
    target_name = found->name;
  }
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (!subsystem.initialized) return FailedPrecondition("audio subsystem stopped during the device switch");
  const bool follow = id.empty();
  if (render) {
    struct Migrated {
      std::shared_ptr<SinkState> state;
      bool follow_default;
      std::wstring id;
    };
    std::vector<Migrated> migrated;
    for (const auto& sink : subsystem.sinks) {
      Migrated previous{sink, true, {}};
      RouteOf(sink, &previous.follow_default, &previous.id);
      const Status status = RequestSwitch(sink, follow, id);
      if (!status.ok()) {
        for (auto rollback = migrated.rbegin(); rollback != migrated.rend(); ++rollback) {
          (void)RequestSwitch(rollback->state, rollback->follow_default, rollback->id, true);
        }
        return status;
      }
      migrated.push_back(std::move(previous));
    }
    subsystem.playback_id = id;
    subsystem.playback_name = target_name;
  } else {
    struct Migrated {
      std::shared_ptr<CaptureState> state;
      bool follow_default;
      std::wstring id;
    };
    std::vector<Migrated> migrated;
    for (const auto& capture : subsystem.captures) {
      Migrated previous{capture, true, {}};
      RouteOf(capture, &previous.follow_default, &previous.id);
      const Status status = RequestSwitch(capture, follow, id);
      if (!status.ok()) {
        for (auto rollback = migrated.rbegin(); rollback != migrated.rend(); ++rollback) {
          (void)RequestSwitch(rollback->state, rollback->follow_default, rollback->id, true);
        }
        return status;
      }
      migrated.push_back(std::move(previous));
    }
    subsystem.recording_id = id;
    subsystem.recording_name = target_name;
  }
  *resolved_name = target_name;
  return Status::Ok();
}

}

std::string WideToUtf8(std::wstring_view text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                       nullptr, nullptr);
  std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
  if (size > 0) {
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr,
                        nullptr);
  }
  return result;
}

std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(static_cast<std::size_t>(std::max(size, 0)), L'\0');
  if (size > 0) {
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
  }
  return result;
}

Status InitializeWasapiAudioSubsystem(const WasapiAudioSubsystemOptions& options) {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (subsystem.initialized) return Status::Ok();
  subsystem.options = options;
  const char* driver = std::getenv("ELIXIR_AUDIO_DRIVER");
  if (driver != nullptr && std::strcmp(driver, "null") == 0) subsystem.options.driver = AudioOutputDriver::kNull;
  subsystem.mta_usage = SUCCEEDED(CoIncrementMTAUsage(&subsystem.mta_cookie));
  if (subsystem.options.driver == AudioOutputDriver::kWasapi) {
    HRESULT hr = S_OK;
    RunInMta([&] {
      hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                            IID_PPV_ARGS(&subsystem.enumerator));
      if (SUCCEEDED(hr)) {
        subsystem.notifier = new (std::nothrow) NotificationClient();
        if (subsystem.notifier != nullptr &&
            FAILED(subsystem.enumerator->RegisterEndpointNotificationCallback(subsystem.notifier))) {
          subsystem.notifier->Release();
          subsystem.notifier = nullptr;
        }
      }
    });
    if (FAILED(hr)) {
      if (subsystem.mta_usage) CoDecrementMTAUsage(subsystem.mta_cookie);
      subsystem.mta_usage = false;
      return HresultStatus("CoCreateInstance(MMDeviceEnumerator)", hr);
    }
    if (subsystem.notifier == nullptr) {
      LogLine("default-device notifications are unavailable; streams stay on their endpoint");
    }
  }
  subsystem.playback_id.clear();
  subsystem.playback_name = "default";
  subsystem.recording_id.clear();
  subsystem.recording_name = "default";
  subsystem.initialized = true;
  return Status::Ok();
}

Status ShutdownWasapiAudioSubsystem() {
  ComPtr<IMMDeviceEnumerator> enumerator;
  NotificationClient* notifier = nullptr;
  bool mta_usage = false;
  CO_MTA_USAGE_COOKIE cookie{};
  {
    Subsystem& subsystem = State();
    std::lock_guard<std::mutex> lock(subsystem.mutex);
    if (!subsystem.initialized) return Status::Ok();
    if (!subsystem.sinks.empty()) return FailedPrecondition("audio shutdown requires every sink to close");
    if (!subsystem.captures.empty()) return FailedPrecondition("audio shutdown requires every capture to close");
    enumerator = std::move(subsystem.enumerator);
    notifier = subsystem.notifier;
    subsystem.notifier = nullptr;
    mta_usage = subsystem.mta_usage;
    cookie = subsystem.mta_cookie;
    subsystem.mta_usage = false;
    subsystem.initialized = false;
    subsystem.playback_id.clear();
    subsystem.playback_name = "default";
    subsystem.recording_id.clear();
    subsystem.recording_name = "default";
  }
  RunInMta([&] {
    if (enumerator != nullptr && notifier != nullptr) {
      enumerator->UnregisterEndpointNotificationCallback(notifier);
    }
    if (notifier != nullptr) notifier->Release();
    enumerator.Reset();
  });
  if (mta_usage) CoDecrementMTAUsage(cookie);
  return Status::Ok();
}

bool WasapiAudioSubsystemInitialized() {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  return subsystem.initialized;
}

AudioOutputDriver WasapiAudioDriver() {
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  return subsystem.options.driver;
}

Status ListWasapiPlaybackDevices(std::vector<WasapiAudioDevice>* devices) {
  if (devices == nullptr) return InvalidArgument("playback device output pointer is null");
  devices->clear();
  if (!WasapiAudioSubsystemInitialized()) {
    return FailedPrecondition("the audio subsystem must be initialized before device enumeration");
  }
  if (WasapiAudioDriver() == AudioOutputDriver::kNull) return Status::Ok();
  return ListDevices(eRender, devices);
}

Status ListWasapiRecordingDevices(std::vector<WasapiAudioDevice>* devices) {
  if (devices == nullptr) return InvalidArgument("recording device output pointer is null");
  devices->clear();
  if (!WasapiAudioSubsystemInitialized()) {
    return FailedPrecondition("the audio subsystem must be initialized before device enumeration");
  }
  if (WasapiAudioDriver() == AudioOutputDriver::kNull) return Status::Ok();
  return ListDevices(eCapture, devices);
}

bool HasWasapiCaptureEndpoint() noexcept {
  try {
    if (!WasapiAudioSubsystemInitialized() || WasapiAudioDriver() != AudioOutputDriver::kWasapi) return false;
    bool present = false;
    RunInMta([&] {
      Subsystem& subsystem = State();
      ComPtr<IMMDeviceEnumerator> enumerator;
      {
        std::lock_guard<std::mutex> lock(subsystem.mutex);
        enumerator = subsystem.enumerator;
      }
      ComPtr<IMMDeviceCollection> endpoints;
      UINT count = 0;
      present = enumerator != nullptr &&
                SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &endpoints)) &&
                SUCCEEDED(endpoints->GetCount(&count)) && count != 0;
    });
    return present;
  } catch (...) {
    return false;
  }
}

Status ResolveWasapiDevice(std::string_view requested, const std::vector<WasapiAudioDevice>& available_devices,
                           std::wstring* endpoint_id, std::string* resolved_name, bool allow_device_name) {
  if (endpoint_id == nullptr || resolved_name == nullptr) {
    return InvalidArgument("audio device selection output pointer is null");
  }
  endpoint_id->clear();
  resolved_name->clear();
  if (requested.empty() || requested.size() > 512U) {
    return InvalidArgument("audio device must be `default`, an endpoint ID or a bounded device name");
  }
  for (const unsigned char character : requested) {
    if (character < 0x20U || character == 0x7fU) {
      return InvalidArgument("audio device selection must not contain control bytes");
    }
  }
  if (requested == "default") {
    *resolved_name = "default";
    return Status::Ok();
  }
  const std::wstring wide = Utf8ToWide(requested);
  for (const WasapiAudioDevice& device : available_devices) {
    if (device.id == wide) {
      *endpoint_id = device.id;
      *resolved_name = device.name;
      return Status::Ok();
    }
  }
  if (allow_device_name) {
    const WasapiAudioDevice* match = nullptr;
    for (const WasapiAudioDevice& device : available_devices) {
      if (device.name != requested) continue;
      if (match != nullptr) return InvalidArgument("audio device name is ambiguous; use its endpoint ID");
      match = &device;
    }
    if (match != nullptr) {
      *endpoint_id = match->id;
      *resolved_name = match->name;
      return Status::Ok();
    }
  }
  return Status::Error(StatusCode::kUnavailable, "configured audio device is unavailable");
}

Status ConfigureWasapiPlaybackDevice(std::string_view requested, std::vector<WasapiAudioDevice>* available_devices,
                                     std::string* resolved_name) {
  return ConfigureDevice(true, requested, available_devices, resolved_name, true);
}

Status ConfigureWasapiRecordingDevice(std::string_view requested, std::vector<WasapiAudioDevice>* available_devices,
                                       std::string* resolved_name, bool allow_device_name) {
  return ConfigureDevice(false, requested, available_devices, resolved_name, allow_device_name);
}

Status GetConfiguredWasapiPlaybackDevice(std::wstring* endpoint_id, std::string* resolved_name) {
  return GetConfigured(true, endpoint_id, resolved_name);
}

Status GetConfiguredWasapiRecordingDevice(std::wstring* endpoint_id, std::string* resolved_name) {
  return GetConfigured(false, endpoint_id, resolved_name);
}

Status SwitchWasapiPlaybackDevice(std::wstring_view endpoint_id, std::string* resolved_name) {
  return SwitchDevice(true, endpoint_id, resolved_name);
}

Status SwitchWasapiRecordingDevice(std::wstring_view endpoint_id, std::string* resolved_name) {
  return SwitchDevice(false, endpoint_id, resolved_name);
}

Status CreateWasapiAudioSink(const WasapiAudioSinkOptions& options, std::unique_ptr<AudioSink>* sink) {
  if (sink == nullptr) return InvalidArgument("audio sink output pointer is null");
  sink->reset();
  Status status = ValidatePcmSpec(options.source_spec);
  if (!status.ok()) return status;
  bool follow_default = true;
  std::wstring route_id;
  status = RouteForTarget(options.target, true, &follow_default, &route_id);
  if (!status.ok()) return status;
  WasapiAudioSubsystemOptions subsystem_options;
  {
    Subsystem& subsystem = State();
    std::lock_guard<std::mutex> lock(subsystem.mutex);
    if (!subsystem.initialized) return FailedPrecondition("the audio subsystem stopped while opening a sink");
    subsystem_options = subsystem.options;
  }

  std::shared_ptr<SinkState> state;
  try {
    state = std::make_shared<SinkState>(options.source_spec);
  } catch (const std::bad_alloc&) {
    return Status::Error(StatusCode::kUnavailable, "unable to allocate the audio sink");
  }
  state->follow_default = follow_default;
  state->route_id = route_id;
  state->want_running = !options.start_paused;
  state->driver = subsystem_options.driver;
  state->mmcss = subsystem_options.mmcss;
  state->low_latency = subsystem_options.low_latency;
  state->enters_guest = options.render_thread_enters_guest;
  state->queue.SetPlaying(!options.start_paused);
  if (options.data_needed_callback != nullptr) {
    state->queue.SetDataNeededCallback(options.data_needed_callback, options.data_needed_context);
  }
  state->command = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (state->command == nullptr) return Status::Error(StatusCode::kPlatformError, "CreateEvent failed");

  auto* holder = new (std::nothrow) std::shared_ptr<SinkState>(state);
  if (holder == nullptr) {
    return Status::Error(StatusCode::kUnavailable, "unable to allocate the audio sink thread state");
  }
  state->thread = CreateGuestCapableThread(&SinkThreadMain, holder, &state->thread_id);
  if (state->thread == nullptr) {
    delete holder;
    return Status::Error(StatusCode::kPlatformError, "unable to create the audio render thread");
  }
  {
    std::unique_lock<std::mutex> lock(state->mutex);
    state->cv.wait(lock, [&state] { return state->init_done; });
    status = state->init_status;
  }
  if (!status.ok()) {
    WaitForSingleObject(state->thread, INFINITE);
    CloseHandle(state->thread);
    state->thread = nullptr;
    return status;
  }
  auto* implementation = new (std::nothrow) WasapiAudioSink(state);
  if (implementation == nullptr) {
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->stop = true;
    }
    state->Signal();
    WaitForSingleObject(state->thread, INFINITE);
    CloseHandle(state->thread);
    state->thread = nullptr;
    return Status::Error(StatusCode::kUnavailable, "unable to allocate the audio sink");
  }
  RegisterSink(state);
  sink->reset(implementation);
  return Status::Ok();
}

Status GetWasapiSinkInfo(const AudioSink* sink, WasapiStreamInfo* info) {
  const auto* wasapi = dynamic_cast<const WasapiAudioSink*>(sink);
  if (wasapi == nullptr || info == nullptr) return InvalidArgument("not a WASAPI audio sink");
  SinkState& state = *wasapi->state();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    *info = state.info;
  }
  info->queue = state.queue.stats();
  info->queued_source_bytes = state.queue.QueuedBytes();
  info->converter_buffered_frames = state.queue.ConverterBufferedFrames();
  return Status::Ok();
}

Status CreateWasapiAudioCapture(const WasapiAudioCaptureOptions& options, std::unique_ptr<AudioCapture>* capture) {
  if (capture == nullptr) return InvalidArgument("audio capture output pointer is null");
  capture->reset();
  if (!MicrophoneCapturePermitted()) {
    return FailedPrecondition("microphone capture is not permitted by the runtime policy");
  }
  Status status = ValidatePcmSpec(options.output_spec);
  if (!status.ok()) return status;
  if (options.data_callback == nullptr || options.frame_duration_ms == 0 || options.frame_duration_ms > 1000 ||
      (static_cast<std::uint64_t>(options.output_spec.sample_rate_hz) * options.frame_duration_ms) % 1000U != 0) {
    return InvalidArgument("audio capture requires a valid fixed-frame callback");
  }
  const std::uint64_t frames =
      static_cast<std::uint64_t>(options.output_spec.sample_rate_hz) * options.frame_duration_ms / 1000U;
  const std::uint64_t bytes = frames * BytesPerFrame(options.output_spec);
  if (frames == 0 || bytes == 0 || bytes > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
    return InvalidArgument("audio capture frame size is invalid");
  }
  bool follow_default = true;
  std::wstring route_id;
  status = RouteForTarget(options.target, false, &follow_default, &route_id);
  if (!status.ok()) return status;
  WasapiAudioSubsystemOptions subsystem_options;
  {
    Subsystem& subsystem = State();
    std::lock_guard<std::mutex> lock(subsystem.mutex);
    if (!subsystem.initialized) return FailedPrecondition("the audio subsystem stopped while opening capture");
    subsystem_options = subsystem.options;
  }
  std::shared_ptr<CaptureState> state;
  try {
    state = std::make_shared<CaptureState>();
    state->buffer.resize(static_cast<std::size_t>(bytes));
  } catch (const std::bad_alloc&) {
    return Status::Error(StatusCode::kUnavailable, "unable to allocate the audio capture");
  }
  state->output_spec = options.output_spec;
  state->frames_per_buffer = static_cast<std::size_t>(frames);
  state->callback = options.data_callback;
  state->callback_context = options.data_context;
  state->follow_default = follow_default;
  state->route_id = route_id;
  state->driver = subsystem_options.driver;
  state->mmcss = subsystem_options.mmcss;
  state->enters_guest = options.capture_thread_enters_guest;
  state->command = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (state->command == nullptr) return Status::Error(StatusCode::kPlatformError, "CreateEvent failed");
  auto* holder = new (std::nothrow) std::shared_ptr<CaptureState>(state);
  if (holder == nullptr) return Status::Error(StatusCode::kUnavailable, "unable to allocate the capture thread state");
  state->thread = CreateGuestCapableThread(&CaptureThreadMain, holder, &state->thread_id);
  if (state->thread == nullptr) {
    delete holder;
    return Status::Error(StatusCode::kPlatformError, "unable to create the audio capture thread");
  }
  {
    std::unique_lock<std::mutex> lock(state->mutex);
    state->cv.wait(lock, [&state] { return state->init_done; });
    status = state->init_status;
  }
  if (!status.ok()) {
    WaitForSingleObject(state->thread, INFINITE);
    CloseHandle(state->thread);
    state->thread = nullptr;
    return status;
  }
  auto* implementation = new (std::nothrow) WasapiAudioCapture(state);
  if (implementation == nullptr) {
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->stop = true;
    }
    state->Signal();
    WaitForSingleObject(state->thread, INFINITE);
    CloseHandle(state->thread);
    state->thread = nullptr;
    return Status::Error(StatusCode::kUnavailable, "unable to allocate the audio capture");
  }
  {
    Subsystem& subsystem = State();
    std::lock_guard<std::mutex> lock(subsystem.mutex);
    subsystem.captures.push_back(state);
  }
  capture->reset(implementation);
  return Status::Ok();
}

Status GetWasapiCaptureInfo(const AudioCapture* capture, WasapiCaptureInfo* info) {
  const auto* wasapi = dynamic_cast<const WasapiAudioCapture*>(capture);
  if (wasapi == nullptr || info == nullptr) return InvalidArgument("not a WASAPI audio capture");
  CaptureState& state = *wasapi->state();
  std::lock_guard<std::mutex> lock(state.mutex);
  *info = state.info;
  return Status::Ok();
}

void SetMicrophoneCapturePermitted(bool permitted) {
  const bool previous = g_microphone_permitted.exchange(permitted, std::memory_order_acq_rel);
  if (previous == permitted || permitted) return;
  std::vector<std::shared_ptr<CaptureState>> captures;
  {
    Subsystem& subsystem = State();
    std::lock_guard<std::mutex> lock(subsystem.mutex);
    captures = subsystem.captures;
  }
  for (const auto& capture : captures) {
    {
      std::lock_guard<std::mutex> lock(capture->mutex);
      capture->want_running = false;
      ++capture->run_request;
    }
    capture->Signal();
    WaitForRunState(*capture);
  }
  LogLine("microphone capture permission revoked; %zu capture stream(s) stopped", captures.size());
}

bool MicrophoneCapturePermitted() { return g_microphone_permitted.load(std::memory_order_acquire); }

void NotifyWasapiDefaultDeviceChanged(bool render, std::wstring_view new_default_id, bool force_reopen) {
  (void)new_default_id;
  Subsystem& subsystem = State();
  std::lock_guard<std::mutex> lock(subsystem.mutex);
  if (render) {
    for (const auto& sink : subsystem.sinks) {
      std::lock_guard<std::mutex> sink_lock(sink->mutex);
      ++sink->info.default_change_notifications;
      if (!sink->follow_default) continue;
      sink->reevaluate = true;
      sink->force_reopen = sink->force_reopen || force_reopen;
      sink->Signal();
    }
  } else {
    for (const auto& capture : subsystem.captures) {
      std::lock_guard<std::mutex> capture_lock(capture->mutex);
      if (!capture->follow_default) continue;
      capture->reevaluate = true;
      capture->force_reopen = capture->force_reopen || force_reopen;
      capture->Signal();
    }
  }
}

}
