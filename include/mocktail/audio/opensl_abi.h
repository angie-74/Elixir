// Modified by Elixir.
#ifndef MOCKTAIL_AUDIO_OPENSL_ABI_H_
#define MOCKTAIL_AUDIO_OPENSL_ABI_H_

#include <cstdint>

#if defined(MOCKTAIL_USE_SYSTEM_OPENSL_HEADERS)
#if !defined(__ANDROID__) && !defined(SL_API_DEPRECATED)
#define SL_API_DEPRECATED(level)
#endif
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#define MOCKTAIL_OPENSL_API_ENTRY SLAPIENTRY
#endif

namespace mocktail::audio::opensl_abi {

#if defined(MOCKTAIL_USE_SYSTEM_OPENSL_HEADERS)

using Result = SLresult;
using Uint32 = SLuint32;
using AndroidSimpleBufferQueue = SLAndroidSimpleBufferQueueItf;
using AndroidSimpleBufferQueueTable = struct SLAndroidSimpleBufferQueueItf_;
using AndroidSimpleBufferQueueState = SLAndroidSimpleBufferQueueState;
using AndroidSimpleBufferQueueCallback = slAndroidSimpleBufferQueueCallback;

inline constexpr Result kResultSuccess = SL_RESULT_SUCCESS;
inline constexpr Result kResultPreconditionsViolated =
    SL_RESULT_PRECONDITIONS_VIOLATED;
inline constexpr Result kResultParameterInvalid = SL_RESULT_PARAMETER_INVALID;
inline constexpr Result kResultMemoryFailure = SL_RESULT_MEMORY_FAILURE;
inline constexpr Result kResultResourceError = SL_RESULT_RESOURCE_ERROR;
inline constexpr Result kResultIoError = SL_RESULT_IO_ERROR;
inline constexpr Result kResultBufferInsufficient =
    SL_RESULT_BUFFER_INSUFFICIENT;
inline constexpr Result kResultFeatureUnsupported =
    SL_RESULT_FEATURE_UNSUPPORTED;
inline constexpr Result kResultInternalError = SL_RESULT_INTERNAL_ERROR;

#else

#if defined(_WIN32)
#define MOCKTAIL_OPENSL_API_ENTRY __attribute__((sysv_abi))
#else
#define MOCKTAIL_OPENSL_API_ENTRY
#endif

using Result = std::uint32_t;
using Uint32 = std::uint32_t;

inline constexpr Result kResultSuccess = 0;
inline constexpr Result kResultPreconditionsViolated = 1;
inline constexpr Result kResultParameterInvalid = 2;
inline constexpr Result kResultMemoryFailure = 3;
inline constexpr Result kResultResourceError = 4;
inline constexpr Result kResultIoError = 6;
inline constexpr Result kResultBufferInsufficient = 7;
inline constexpr Result kResultFeatureUnsupported = 12;
inline constexpr Result kResultInternalError = 13;

struct AndroidSimpleBufferQueueState {
  Uint32 count;
  Uint32 index;
};

struct AndroidSimpleBufferQueueTable;
using AndroidSimpleBufferQueue = const AndroidSimpleBufferQueueTable* const*;
using AndroidSimpleBufferQueueCallback =
    void (MOCKTAIL_OPENSL_API_ENTRY*)(AndroidSimpleBufferQueue caller,
                                      void* context);

struct AndroidSimpleBufferQueueTable {
  Result (MOCKTAIL_OPENSL_API_ENTRY* Enqueue)(AndroidSimpleBufferQueue self,
                                              const void* buffer, Uint32 size);
  Result (MOCKTAIL_OPENSL_API_ENTRY* Clear)(AndroidSimpleBufferQueue self);
  Result (MOCKTAIL_OPENSL_API_ENTRY* GetState)(
      AndroidSimpleBufferQueue self, AndroidSimpleBufferQueueState* state);
  Result (MOCKTAIL_OPENSL_API_ENTRY* RegisterCallback)(
      AndroidSimpleBufferQueue self, AndroidSimpleBufferQueueCallback callback,
      void* context);
};

#if defined(_WIN32)
static_assert(sizeof(AndroidSimpleBufferQueueState) == 8,
              "SLAndroidSimpleBufferQueueState layout");
static_assert(sizeof(AndroidSimpleBufferQueueTable) == 4 * sizeof(void*),
              "SLAndroidSimpleBufferQueueItf_ layout");
#endif

#endif

}

#endif
