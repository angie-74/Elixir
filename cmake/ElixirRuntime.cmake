# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

include(FetchContent)
if(NOT TARGET nlohmann_json::nlohmann_json)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  set(JSON_Install OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(elixir_runtime_mocktail STATIC
  src/runtime/environment.cc
  src/runtime/command_line.cc
  src/runtime/device_profile.cc
  src/runtime/device_memory_profile.cc
  src/runtime/runtime_config.cc
  src/runtime/runtime_config_bootstrap.cc
  src/runtime/runtime_config_file.cc
  src/runtime/runtime_paths.cc
  src/runtime/session_log.cc
  src/runtime/session_log_windows.cc
  src/runtime/performance_policy.cc
  src/runtime/frame_rate_policy.cc
  src/runtime/crash_report_policy.cc
  src/runtime/roblox_desktop_app_policy.cc
  src/runtime/texture_memory_policy.cc
  src/runtime/http_client_policy.cc
  src/runtime/memory_limit.cc
  src/runtime/process_diagnostics.cc
  src/services/client_settings_service.cc
  src/services/auth_service.cc
  src/elixir_runtime/game_mode_policy_windows.cc
)
elixir_target(elixir_runtime_mocktail)
target_include_directories(elixir_runtime_mocktail PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(elixir_runtime_mocktail PRIVATE
  "MOCKTAIL_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "MOCKTAIL_BUILD_GIT_COMMIT=\"${MOCKTAIL_BUILD_GIT_COMMIT}\""
  "MOCKTAIL_BUILD_GIT_DIRTY=\"${MOCKTAIL_BUILD_GIT_DIRTY}\""
  "MOCKTAIL_BUILD_TYPE=\"$<CONFIG>\""
  "MOCKTAIL_DISCORD_APPLICATION_ID=\"${MOCKTAIL_DISCORD_APPLICATION_ID}\"")
target_link_libraries(elixir_runtime_mocktail PUBLIC nlohmann_json::nlohmann_json psapi user32 shell32 ole32)

add_library(elixir_runtime_core STATIC
  src/elixir_runtime/cli.cc
  src/elixir_runtime/elf_dynamic.cc
  src/elixir_runtime/guest_environment.cc
  src/elixir_runtime/guest_symbolizer.cc
  src/elixir_runtime/host_log.cc
  src/elixir_runtime/import_check.cc
  src/elixir_runtime/link_joins.cc
  src/elixir_runtime/network_trace.cc
  src/elixir_runtime/payload.cc
  src/elixir_runtime/process_tuning.cc
  src/elixir_runtime/relaunch.cc
  src/elixir_runtime/runtime_flags.cc
  src/elixir_runtime/runtime_main.cc
  src/elixir_runtime/text.cc
  src/elixir_runtime/winhttp_client.cc
)
elixir_target(elixir_runtime_core)
target_link_libraries(elixir_runtime_core PUBLIC
  elixir_runtime_mocktail elixir_bionic elixir_fflags elixir_platform_windows avrt winmm winhttp crypt32)
if(TARGET elixir_vulkan)
  target_sources(elixir_runtime_core PRIVATE src/elixir_runtime/gpu_memory.cc)
  target_link_libraries(elixir_runtime_core PUBLIC elixir_vulkan)
  target_link_libraries(elixir_runtime_core PRIVATE dxgi)
  target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_VULKAN=1)
endif()
if(TARGET elixir_jnivm)
  target_sources(elixir_runtime_core PRIVATE
    src/elixir_runtime/app_restart.cc
    src/elixir_runtime/roblox_startup.cc
    src/elixir_runtime/sign_in_identity.cc
    src/elixir_runtime/roblox_session_store.cc)
  target_link_libraries(elixir_runtime_core PUBLIC elixir_jnivm)
  target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_JNI=1)
endif()
get_target_property(_elixir_bionic_sources elixir_bionic SOURCES)
if(NOT _elixir_bionic_sources MATCHES "guest_ext\\.cc")
  target_sources(elixir_runtime_core PRIVATE src/bionic/guest_ext.cc)
endif()

add_library(elixir_runtime_host STATIC
  src/elixir_core/core_api.cc
  src/elixir_runtime/host_main.cc
)
elixir_target(elixir_runtime_host)
target_include_directories(elixir_runtime_host PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(elixir_runtime_host PRIVATE
  "ELIXIR_CORE_BUILD=\"${MOCKTAIL_BUILD_GIT_COMMIT} $<CONFIG> clang ${CMAKE_CXX_COMPILER_VERSION}\"")
target_link_libraries(elixir_runtime_host PUBLIC elixir_runtime_core shell32)

if(BUILD_TESTING)
  enable_language(RC)
  file(TO_NATIVE_PATH "${ELIXIR_ICON_ICO}" _elixir_runtime_rc_icon)
  string(REPLACE "\\" "\\\\" ELIXIR_RC_ICON "${_elixir_runtime_rc_icon}")
  set(ELIXIR_RC_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(ELIXIR_RC_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(ELIXIR_RC_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  set(ELIXIR_RUNTIME_RC "${CMAKE_BINARY_DIR}/elixir-runtime.rc")
  configure_file("${CMAKE_SOURCE_DIR}/src/elixir_runtime/elixir-runtime.rc.in" "${ELIXIR_RUNTIME_RC}" @ONLY)
  set_source_files_properties("${ELIXIR_RUNTIME_RC}" PROPERTIES OBJECT_DEPENDS "${ELIXIR_ICON_ICO}")
  add_executable(elixir_runtime src/elixir_runtime/host_entry.cc "${ELIXIR_RUNTIME_RC}")
  elixir_target(elixir_runtime)
  set_target_properties(elixir_runtime PROPERTIES OUTPUT_NAME elixir-runtime)
  target_link_libraries(elixir_runtime PRIVATE elixir_runtime_host)
  add_dependencies(elixir_runtime elixir_branding)
  target_link_options(elixir_runtime PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /STACK:16777216)
endif()

if(ELIXIR_GUEST_RUNTIME_DIR AND IS_DIRECTORY "${ELIXIR_GUEST_RUNTIME_DIR}")
  file(GLOB _elixir_guest_runtime_libraries "${ELIXIR_GUEST_RUNTIME_DIR}/*.so")
  if(_elixir_guest_runtime_libraries)
    add_custom_target(elixir_guest_runtime_folder ALL
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/guest-runtime"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${_elixir_guest_runtime_libraries}
              "${CMAKE_BINARY_DIR}/guest-runtime"
      COMMENT "Copying the guest runtime libraries to guest-runtime"
      VERBATIM)
    if(TARGET elixir_runtime)
      add_dependencies(elixir_runtime elixir_guest_runtime_folder)
    endif()
  endif()
endif()

if(NOT BUILD_TESTING)
  return()
endif()

set(ELIXIR_RUNTIME_TESTS "${CMAKE_SOURCE_DIR}/tests/windows/runtime")

elixir_gtest(elixir_runtime_mocktail_policy_test
  tests/frame_rate_policy_test.cc
  tests/http_client_policy_test.cc
  tests/texture_memory_policy_test.cc
  tests/crash_report_policy_test.cc
  tests/performance_policy_test.cc)
target_link_libraries(elixir_runtime_mocktail_policy_test PRIVATE elixir_runtime_mocktail)
set_source_files_properties(tests/crash_report_policy_test.cc PROPERTIES
  COMPILE_OPTIONS "/FI${ELIXIR_RUNTIME_TESTS}/mocktail_test_compat.h")

elixir_gtest(elixir_runtime_test
  "${ELIXIR_RUNTIME_TESTS}/cli_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/config_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/network_trace_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/policy_flags_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/process_tuning_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/relaunch_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/session_log_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/guest_runtime_test.cc"
  "${ELIXIR_RUNTIME_TESTS}/core_host_test.cc")
target_link_libraries(elixir_runtime_test PRIVATE elixir_runtime_core)
add_dependencies(elixir_runtime_test elixir_runtime)
target_compile_definitions(elixir_runtime_test PRIVATE
  "ELIXIR_SOURCE_DIR_DEFINE=\"${CMAKE_SOURCE_DIR}\""
  "ELIXIR_GUEST_TEST_DIR=\"${ELIXIR_GUEST_TEST_DIR}\""
  "ELIXIR_GUEST_RUNTIME_DIR=\"${ELIXIR_GUEST_RUNTIME_DIR}\""
  "ELIXIR_RUNTIME_EXE=\"$<TARGET_FILE:elixir_runtime>\"")

if(TARGET elixir_jnivm)
  elixir_gtest(elixir_runtime_identity_test
    tests/auth_service_test.cc
    "${ELIXIR_RUNTIME_TESTS}/app_restart_test.cc"
    "${ELIXIR_RUNTIME_TESTS}/sign_in_identity_test.cc")
  target_link_libraries(elixir_runtime_identity_test PRIVATE elixir_runtime_core)
endif()
