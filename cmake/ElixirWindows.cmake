# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR
    "Elixir on Windows must be compiled with clang-cl: guest-facing code uses "
    "__attribute__((sysv_abi)), which MSVC cannot emit. Run tools/build.ps1.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Elixir requires an x86-64 (64-bit) Windows target")
endif()

enable_language(C)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/tests")
  set(BUILD_TESTING OFF CACHE BOOL "Build the tests" FORCE)
endif()
if(DEFINED BUILD_TESTING AND NOT BUILD_TESTING)
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/clang:-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.>")
endif()
set(ELIXIR_VERSION "${PROJECT_VERSION}")
set(ELIXIR_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

add_library(elixir_compile_options INTERFACE)
target_compile_definitions(elixir_compile_options INTERFACE
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00
  ELIXIR_VERSION="${ELIXIR_VERSION}")
target_compile_options(elixir_compile_options INTERFACE
  /W4 /permissive- /Zc:__cplusplus /EHsc /utf-8
  /clang:-mno-red-zone
  -Wno-unused-parameter -Wno-missing-field-initializers
  -Wno-gnu-zero-variadic-macro-arguments
  /Oy-
  /Brepro
  $<$<CONFIG:Release,RelWithDebInfo>:/O2 /Gw /Gy>
  $<$<CONFIG:Release>:/Zi>)
target_include_directories(elixir_compile_options INTERFACE
  "${CMAKE_SOURCE_DIR}/src")

option(ELIXIR_ENABLE_LTO "Enable link-time optimization (ThinLTO) for release builds" OFF)
if(ELIXIR_ENABLE_LTO)
  target_compile_options(elixir_compile_options INTERFACE
    $<$<CONFIG:Release,RelWithDebInfo>:-flto=thin>)
endif()

function(elixir_target target_name)
  target_link_libraries(${target_name} PRIVATE elixir_compile_options)
  set_target_properties(${target_name} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  get_target_property(_type ${target_name} TYPE)
  if(_type STREQUAL "EXECUTABLE")
    target_sources(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/cmake/elixir.manifest")
    target_link_options(${target_name} PRIVATE /CETCOMPAT:NO /guard:no /DYNAMICBASE /HIGHENTROPYVA)
  elseif(_type STREQUAL "SHARED_LIBRARY")
    target_link_options(${target_name} PRIVATE /guard:no /DYNAMICBASE /HIGHENTROPYVA)
  endif()
  target_link_options(${target_name} PRIVATE /Brepro $<$<CONFIG:Release,RelWithDebInfo>:/OPT:REF /OPT:ICF /INCREMENTAL:NO>
    $<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>)
endfunction()

add_library(elixir_abi STATIC
  src/abi/long_double.cc
)
elixir_target(elixir_abi)

add_library(elixir_platform_windows STATIC
  src/platform/windows/guest_tls.cc
  src/platform/windows/crash_reporter.cc
  src/platform/windows/guest_unwind_barrier.cc
  src/platform/windows/raw_syscall_scan.cc
  src/platform/windows/raw_syscall_trap.cc
)
elixir_target(elixir_platform_windows)
target_link_libraries(elixir_platform_windows PUBLIC elixir_abi bcrypt dbghelp)

add_executable(elixir_svg_rasterize tools/svg_rasterize/svg_rasterize.cc)
elixir_target(elixir_svg_rasterize)
target_link_libraries(elixir_svg_rasterize PRIVATE d2d1 d3d11 dxgi windowscodecs shlwapi ole32)
target_link_options(elixir_svg_rasterize PRIVATE /ENTRY:wmainCRTStartup)
set(ELIXIR_BRANDING_DIR "${CMAKE_BINARY_DIR}/branding")
set(ELIXIR_ICON_SVG "${CMAKE_SOURCE_DIR}/assets/branding/elixir-icon.svg")
set(ELIXIR_LOGO_SVG "${CMAKE_SOURCE_DIR}/assets/branding/elixir-logo.svg")
set(ELIXIR_ICON_ICO "${ELIXIR_BRANDING_DIR}/elixir.ico")
add_custom_command(
  OUTPUT "${ELIXIR_ICON_ICO}"
         "${ELIXIR_BRANDING_DIR}/elixir-icon-256.png"
         "${ELIXIR_BRANDING_DIR}/elixir-icon-64.png"
         "${ELIXIR_BRANDING_DIR}/elixir-logo-886x320.png"
         "${ELIXIR_BRANDING_DIR}/elixir-logo-443x160.png"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${ELIXIR_BRANDING_DIR}"
  COMMAND elixir_svg_rasterize ico "${ELIXIR_ICON_SVG}" "${ELIXIR_ICON_ICO}" 16,20,24,32,40,48,64,96,128,256
  COMMAND elixir_svg_rasterize png "${ELIXIR_ICON_SVG}" "${ELIXIR_BRANDING_DIR}/elixir-icon-256.png" 256
  COMMAND elixir_svg_rasterize png "${ELIXIR_ICON_SVG}" "${ELIXIR_BRANDING_DIR}/elixir-icon-64.png" 64
  COMMAND elixir_svg_rasterize png "${ELIXIR_LOGO_SVG}" "${ELIXIR_BRANDING_DIR}/elixir-logo-886x320.png" 886 320
  COMMAND elixir_svg_rasterize png "${ELIXIR_LOGO_SVG}" "${ELIXIR_BRANDING_DIR}/elixir-logo-443x160.png" 443 160
  DEPENDS elixir_svg_rasterize "${ELIXIR_ICON_SVG}" "${ELIXIR_LOGO_SVG}"
  COMMENT "Rendering Elixir branding assets"
  VERBATIM)
add_custom_target(elixir_branding ALL DEPENDS "${ELIXIR_ICON_ICO}")

add_library(elixir_posix_host STATIC
  src/platform/windows/posix_compat/posix_compat.cc
)
elixir_target(elixir_posix_host)
target_include_directories(elixir_posix_host PUBLIC
  "${CMAKE_SOURCE_DIR}/src/platform/windows/posix_compat/include")

include(${CMAKE_SOURCE_DIR}/cmake/ElixirLinker.cmake)

add_library(elixir_bionic_runtime STATIC
  src/bionic/runtime.cc
  src/bionic/memory.cc
  src/bionic/wake_trace.cc
)
elixir_target(elixir_bionic_runtime)
target_link_libraries(elixir_bionic_runtime PUBLIC elixir_platform_windows ws2_32 synchronization)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
  add_subdirectory(tests/windows)
endif()

option(ELIXIR_WITH_JNI "Build the JNI layer (libjnivm on System V tables)" ON)
option(ELIXIR_WITH_VULKAN "Build the Vulkan System V thunks" ON)
option(ELIXIR_WITH_APK "Build APK validation and installation" ON)
option(ELIXIR_WITH_LAUNCHER "Build the Elixir.exe launcher" ON)

if(ELIXIR_WITH_JNI)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirJni.cmake OPTIONAL)
endif()

if(ELIXIR_WITH_VULKAN)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirVulkan.cmake OPTIONAL)
endif()

if(ELIXIR_WITH_APK)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirApk.cmake OPTIONAL)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/ElixirFflags.cmake)

if(ELIXIR_WITH_LAUNCHER)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirLauncher.cmake OPTIONAL)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/bionic/ElixirBionicDomain.cmake)
elixir_bionic_domain(text)
elixir_bionic_domain(core DEPENDS elixir_bionic_text elixir_bionic_threads)
elixir_bionic_domain(format DEPENDS elixir_bionic_text elixir_bionic_threads)
elixir_bionic_domain(threads DEPENDS elixir_bionic_text elixir_bionic_core)
elixir_bionic_domain(files DEPENDS elixir_bionic_text elixir_bionic_format elixir_bionic_threads elixir_bionic_core)
elixir_bionic_domain(net DEPENDS elixir_bionic_text elixir_bionic_threads elixir_bionic_files elixir_bionic_core)
if(TARGET elixir_bionic_files_test AND TARGET elixir_bionic_net)
  target_link_libraries(elixir_bionic_files_test PRIVATE elixir_bionic_net)
endif()
elixir_bionic_domain(mm DEPENDS elixir_bionic_text elixir_bionic_core elixir_bionic_files)
if(TARGET elixir_bionic_net)
  target_link_libraries(elixir_bionic_net PUBLIC ws2_32 iphlpapi)
endif()

add_library(elixir_bionic STATIC src/bionic/export_table.cc src/bionic/guest_ext.cc
  src/bionic/dns_config_windows.cc src/bionic/raw_syscalls.cc)
target_compile_definitions(elixir_bionic PUBLIC ELIXIR_BIONIC_GUEST_EXT_LINKED=1)
elixir_target(elixir_bionic)
target_link_libraries(elixir_bionic PUBLIC
  elixir_bionic_runtime elixir_linker iphlpapi ws2_32
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_text>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_core>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_format>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_threads>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_files>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_net>"
  "$<TARGET_NAME_IF_EXISTS:elixir_bionic_mm>")

if(BUILD_TESTING)
  add_executable(elixir_guest_suite_test tests/windows/guest_suite_test.cc tests/windows/guest_resolver_test.cc)
  elixir_target(elixir_guest_suite_test)
  target_link_libraries(elixir_guest_suite_test PRIVATE elixir_bionic GTest::gtest)
  target_compile_definitions(elixir_guest_suite_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}"
    ELIXIR_GUEST_RUNTIME_DIR="${ELIXIR_GUEST_RUNTIME_DIR}")
  add_test(NAME elixir_guest_suite COMMAND elixir_guest_suite_test)

  elixir_gtest(elixir_dns_config_test tests/windows/dns_config_test.cc)
  target_link_libraries(elixir_dns_config_test PRIVATE elixir_bionic)

  elixir_gtest(elixir_raw_syscalls_libc_test tests/windows/raw_syscalls_libc_test.cc)
  target_link_libraries(elixir_raw_syscalls_libc_test PRIVATE elixir_bionic)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/abi_overhead_bench.cc")
  add_executable(elixir_abi_overhead_bench benchmarks/abi_overhead_bench.cc)
  elixir_target(elixir_abi_overhead_bench)
  target_link_libraries(elixir_abi_overhead_bench PRIVATE elixir_linker elixir_bionic_runtime)
  target_compile_definitions(elixir_abi_overhead_bench PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/ElixirRuntime.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/ElixirWindowInput.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/ElixirWebView.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/ElixirAudio.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/ElixirAndroid.cmake)
