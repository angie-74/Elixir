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

FetchContent_GetProperties(zlib)
if(NOT zlib_SOURCE_DIR OR NOT EXISTS "${zlib_SOURCE_DIR}/contrib/minizip/unzip.c")
  message(FATAL_ERROR "ElixirApk.cmake needs zlib's contrib/minizip (fetched by ElixirLinker.cmake)")
endif()
set(ELIXIR_MINIZIP_DIR "${zlib_SOURCE_DIR}/contrib/minizip")

function(elixir_third_party_c target_name)
  set_target_properties(${target_name} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_definitions(${target_name} PRIVATE
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS WIN32_LEAN_AND_MEAN)
  target_compile_options(${target_name} PRIVATE /W0 /utf-8
    $<$<CONFIG:Release,RelWithDebInfo>:/O2>)
endfunction()

add_library(elixir_minizip STATIC
  "${ELIXIR_MINIZIP_DIR}/unzip.c"
  "${ELIXIR_MINIZIP_DIR}/ioapi.c"
  "${ELIXIR_MINIZIP_DIR}/iowin32.c"
)
elixir_third_party_c(elixir_minizip)
target_include_directories(elixir_minizip PUBLIC "${zlib_SOURCE_DIR}/contrib")
target_link_libraries(elixir_minizip PUBLIC zlibstatic)

add_library(elixir_apk STATIC
  src/update/android_manifest.cc
  src/update/apk_bundle.cc
  src/update/apk_provider.cc
  src/update/apk_signature.cc
  src/update/compatibility_catalog.cc
  src/update/payload_integrity.cc
  src/update/zip_archive.cc
  src/compat/elf_build_id.cc
  src/update/apk_signature_crypto_windows.cc
  src/update/cng_crypto_windows.cc
  src/update/host_file_windows.cc
  src/compat/elf_build_id_windows.cc
  src/compat/elf64_reader.cc
)
elixir_target(elixir_apk)
target_include_directories(elixir_apk PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(elixir_apk
  PUBLIC nlohmann_json::nlohmann_json bcrypt crypt32
  PRIVATE elixir_minizip)

add_library(elixir_apk_tool_lib STATIC tools/apk_tool/apk_tool.cc)
elixir_target(elixir_apk_tool_lib)
target_include_directories(elixir_apk_tool_lib PUBLIC "${CMAKE_SOURCE_DIR}/tools/apk_tool")
target_link_libraries(elixir_apk_tool_lib PUBLIC elixir_apk)
if(BUILD_TESTING)
  target_compile_definitions(elixir_apk_tool_lib PRIVATE
    "ELIXIR_APK_DEFAULT_TRUST_MANIFEST=\"${CMAKE_SOURCE_DIR}/config/roblox_signing_certificates.json\""
    "ELIXIR_APK_DEFAULT_COMPATIBILITY_MANIFEST=\"${CMAKE_SOURCE_DIR}/config/roblox_compatibility.json\"")
endif()

if(BUILD_TESTING)
  add_executable(elixir_apk_tool tools/apk_tool/main.cc)
  elixir_target(elixir_apk_tool)
  target_link_libraries(elixir_apk_tool PRIVATE elixir_apk_tool_lib)
  target_link_options(elixir_apk_tool PRIVATE /ENTRY:wmainCRTStartup)
endif()

add_library(elixir_apk_download STATIC
  src/update/http_download_windows.cc
)
elixir_target(elixir_apk_download)
target_link_libraries(elixir_apk_download PUBLIC elixir_apk winhttp)

if(NOT BUILD_TESTING)
  return()
endif()

add_library(elixir_minizip_writer STATIC "${ELIXIR_MINIZIP_DIR}/zip.c")
elixir_third_party_c(elixir_minizip_writer)
target_link_libraries(elixir_minizip_writer PUBLIC elixir_minizip)

add_executable(elixir_apk_fixture_zip tests/windows/apk/fixtures/fixture_zip.cc)
elixir_target(elixir_apk_fixture_zip)
target_link_libraries(elixir_apk_fixture_zip PRIVATE elixir_minizip_writer)
target_link_options(elixir_apk_fixture_zip PRIVATE /ENTRY:wmainCRTStartup)

set(ELIXIR_APK_TEST_DIR "${CMAKE_SOURCE_DIR}/tests/windows/apk")

function(elixir_apk_test name)
  elixir_gtest(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE elixir_apk elixir_minizip_writer)
  target_include_directories(${name} PRIVATE "${ELIXIR_APK_TEST_DIR}")
  target_compile_definitions(${name} PRIVATE
    "ELIXIR_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
    "ELIXIR_GUEST_TEST_DIR=\"${ELIXIR_GUEST_TEST_DIR}\"")
endfunction()

elixir_apk_test(elixir_apk_provider_test tests/apk_provider_test.cc)

elixir_apk_test(elixir_apk_compatibility_catalog_test
  ${ELIXIR_APK_TEST_DIR}/compatibility_catalog_test.cc)
elixir_apk_test(elixir_apk_elf_build_id_test
  ${ELIXIR_APK_TEST_DIR}/elf_build_id_test.cc)
elixir_apk_test(elixir_apk_zip_archive_test
  ${ELIXIR_APK_TEST_DIR}/zip_archive_test.cc)

elixir_apk_test(elixir_apk_android_manifest_test
  ${ELIXIR_APK_TEST_DIR}/android_manifest_test.cc)
elixir_apk_test(elixir_apk_payload_integrity_test
  ${ELIXIR_APK_TEST_DIR}/payload_integrity_test.cc)
elixir_apk_test(elixir_apk_cng_crypto_test
  ${ELIXIR_APK_TEST_DIR}/cng_crypto_test.cc)
target_link_libraries(elixir_apk_cng_crypto_test PRIVATE ncrypt)
elixir_apk_test(elixir_apk_signature_synthetic_test
  ${ELIXIR_APK_TEST_DIR}/apk_signature_synthetic_test.cc)
target_link_libraries(elixir_apk_signature_synthetic_test PRIVATE ncrypt)

set(ELIXIR_APK_FIXTURE_DIR "${CMAKE_BINARY_DIR}/apk_fixtures")
set(_apk_missing "")

set(_sdk_roots "")
foreach(_variable IN ITEMS ANDROID_HOME ANDROID_SDK_ROOT)
  if(DEFINED ENV{${_variable}})
    file(TO_CMAKE_PATH "$ENV{${_variable}}" _root)
    list(APPEND _sdk_roots "${_root}")
  endif()
endforeach()
if(DEFINED ENV{LOCALAPPDATA})
  file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/Android/Sdk" _root)
  list(APPEND _sdk_roots "${_root}")
endif()
set(ELIXIR_ANDROID_BUILD_TOOLS "" CACHE PATH "Android SDK build-tools directory (apksigner, zipalign, aapt2)")
set(ELIXIR_ANDROID_JAR "" CACHE FILEPATH "android.jar used by aapt2 link")
foreach(_root IN LISTS _sdk_roots)
  if(NOT ELIXIR_ANDROID_BUILD_TOOLS)
    file(GLOB _versions LIST_DIRECTORIES true "${_root}/build-tools/*")
    list(SORT _versions COMPARE NATURAL ORDER DESCENDING)
    foreach(_candidate IN LISTS _versions)
      if(EXISTS "${_candidate}/apksigner.bat" AND EXISTS "${_candidate}/zipalign.exe"
         AND EXISTS "${_candidate}/aapt2.exe")
        set(ELIXIR_ANDROID_BUILD_TOOLS "${_candidate}" CACHE PATH "" FORCE)
        break()
      endif()
    endforeach()
  endif()
  if(NOT ELIXIR_ANDROID_JAR)
    file(GLOB _platforms LIST_DIRECTORIES true "${_root}/platforms/android-*")
    list(SORT _platforms COMPARE NATURAL ORDER DESCENDING)
    foreach(_candidate IN LISTS _platforms)
      if(EXISTS "${_candidate}/android.jar")
        set(ELIXIR_ANDROID_JAR "${_candidate}/android.jar" CACHE FILEPATH "" FORCE)
        break()
      endif()
    endforeach()
  endif()
endforeach()
if(NOT ELIXIR_ANDROID_BUILD_TOOLS)
  list(APPEND _apk_missing "Android SDK build-tools (apksigner/zipalign/aapt2)")
endif()
if(NOT ELIXIR_ANDROID_JAR)
  list(APPEND _apk_missing "Android platform android.jar")
endif()
set(_java_hints "")
if(DEFINED ENV{JAVA_HOME})
  file(TO_CMAKE_PATH "$ENV{JAVA_HOME}/bin" _java_hints)
endif()
find_program(ELIXIR_KEYTOOL keytool HINTS ${_java_hints})
if(NOT ELIXIR_KEYTOOL)
  list(APPEND _apk_missing "JDK keytool")
endif()
find_program(ELIXIR_POWERSHELL powershell)
if(NOT ELIXIR_POWERSHELL)
  list(APPEND _apk_missing "powershell")
endif()
get_filename_component(_ndk_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
if(NOT EXISTS "${_ndk_bin}/clang.exe" OR NOT EXISTS "${_ndk_bin}/llvm-readelf.exe")
  list(APPEND _apk_missing "NDK clang.exe / llvm-readelf.exe next to ${CMAKE_CXX_COMPILER}")
endif()
set(_guest_libraries
  "${ELIXIR_GUEST_TEST_DIR}/libelixir_guest_smoke.so"
  "${ELIXIR_GUEST_TEST_DIR}/libelixir_guest_basic.so")
foreach(_library IN LISTS _guest_libraries)
  if(NOT EXISTS "${_library}")
    list(APPEND _apk_missing "${_library} (run tools/build.ps1 -Guest)")
  endif()
endforeach()

if(_apk_missing)
  list(JOIN _apk_missing "; " _apk_missing_text)
  message(WARNING "APK end-to-end fixtures disabled, missing: ${_apk_missing_text}")
  return()
endif()

set(_fixture_sources
  "${CMAKE_SOURCE_DIR}/tools/make_test_apks.ps1"
  "${ELIXIR_APK_TEST_DIR}/fixtures/AndroidManifest.base.xml"
  "${ELIXIR_APK_TEST_DIR}/fixtures/AndroidManifest.split.xml"
  "${ELIXIR_APK_TEST_DIR}/fixtures/fixture_libroblox.c")
add_custom_command(
  OUTPUT "${ELIXIR_APK_FIXTURE_DIR}/fixtures.json"
  COMMAND "${ELIXIR_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
          -File "${CMAKE_SOURCE_DIR}/tools/make_test_apks.ps1"
          -OutputDir "${ELIXIR_APK_FIXTURE_DIR}"
          -FixtureSourceDir "${ELIXIR_APK_TEST_DIR}/fixtures"
          -GuestDir "${ELIXIR_GUEST_TEST_DIR}"
          -ZipTool "$<TARGET_FILE:elixir_apk_fixture_zip>"
          -BuildTools "${ELIXIR_ANDROID_BUILD_TOOLS}"
          -AndroidJar "${ELIXIR_ANDROID_JAR}"
          -Keytool "${ELIXIR_KEYTOOL}"
          -NdkBin "${_ndk_bin}"
  DEPENDS ${_fixture_sources} ${_guest_libraries} elixir_apk_fixture_zip
  COMMENT "Building and signing APK fixtures (aapt2, zipalign, apksigner v2+v3)"
  VERBATIM)
add_custom_target(elixir_apk_fixtures DEPENDS "${ELIXIR_APK_FIXTURE_DIR}/fixtures.json")

elixir_apk_test(elixir_apk_end_to_end_test ${ELIXIR_APK_TEST_DIR}/apk_end_to_end_test.cc)
add_dependencies(elixir_apk_end_to_end_test elixir_apk_fixtures)
target_compile_definitions(elixir_apk_end_to_end_test PRIVATE
  "ELIXIR_APK_FIXTURE_DIR=\"${ELIXIR_APK_FIXTURE_DIR}\"")

elixir_apk_test(elixir_apk_tool_test ${ELIXIR_APK_TEST_DIR}/apk_tool_test.cc)
target_link_libraries(elixir_apk_tool_test PRIVATE elixir_apk_tool_lib)
add_dependencies(elixir_apk_tool_test elixir_apk_fixtures elixir_apk_tool)
target_compile_definitions(elixir_apk_tool_test PRIVATE
  "ELIXIR_APK_FIXTURE_DIR=\"${ELIXIR_APK_FIXTURE_DIR}\""
  "ELIXIR_APK_TOOL_EXE=\"$<TARGET_FILE:elixir_apk_tool>\"")
