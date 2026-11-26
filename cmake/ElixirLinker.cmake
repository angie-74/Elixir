# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

set(ELIXIR_LINKER_DIR "${CMAKE_SOURCE_DIR}/third_party/mcpelauncher-linker")

set(ELIXIR_SYMLINK_ROOT "${CMAKE_BINARY_DIR}/materialized_symlinks")
function(elixir_materialize_git_symlink link)
  set(source "${CMAKE_SOURCE_DIR}/${link}")
  if(IS_DIRECTORY "${source}" OR NOT EXISTS "${source}")
    return()
  endif()
  file(SIZE "${source}" size)
  if(size GREATER 256)
    return()
  endif()
  file(READ "${source}" target)
  string(STRIP "${target}" target)
  if(NOT target MATCHES "^\\.\\.?/" OR target MATCHES "\n")
    return()
  endif()
  get_filename_component(link_dir "${source}" DIRECTORY)
  get_filename_component(resolved "${link_dir}/${target}" ABSOLUTE)
  set(destination "${ELIXIR_SYMLINK_ROOT}/${link}")
  if(IS_DIRECTORY "${resolved}")
    file(COPY "${resolved}/" DESTINATION "${destination}")
  elseif(EXISTS "${resolved}")
    configure_file("${resolved}" "${destination}" COPYONLY)
  endif()
endfunction()
foreach(root IN ITEMS
    third_party/mcpelauncher-linker/include
    third_party/mcpelauncher-linker/core/libcutils/include)
  file(GLOB_RECURSE _candidates RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/${root}/*")
  foreach(candidate IN LISTS _candidates)
    elixir_materialize_git_symlink("${candidate}")
  endforeach()
endforeach()

include(FetchContent)
FetchContent_Declare(
  zlib
  URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib131.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)
set_target_properties(zlibstatic PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_include_directories(zlibstatic PUBLIC "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")

set(_l "${ELIXIR_LINKER_DIR}")
add_library(elixir_linker STATIC
  ${_l}/bionic/linker/rt.cpp
  ${_l}/bionic/linker/linker_gdb_support.cpp
  ${_l}/bionic/libc/bionic/bionic_call_ifunc_resolver.cpp
  ${_l}/bionic/linker/linker_dlwarning.cpp
  ${_l}/bionic/linker/dlfcn.cpp
  ${_l}/bionic/linker/linker_phdr.cpp
  ${_l}/bionic/linker/linker_soinfo.cpp
  ${_l}/bionic/linker/linker.cpp
  ${_l}/bionic/linker/linker_config.cpp
  ${_l}/bionic/linker/linker_utils.cpp
  ${_l}/bionic/linker/linker_debug.cpp
  ${_l}/bionic/linker/linker_block_allocator.cpp
  ${_l}/bionic/linker/linker_mapped_file_fragment.cpp
  ${_l}/bionic/linker/linker_relocate.cpp
  ${_l}/bionic/linker/linker_namespaces.cpp
  ${_l}/bionic/linker/linker_globals.cpp
  ${_l}/bionic/linker/linker_main.cpp
  ${_l}/bionic/linker/linker_cfi.cpp
  ${_l}/bionic/linker/linker_sdk_versions.cpp
  ${_l}/bionic/linker/linker_logger.cpp
  ${_l}/core/base/mapped_file.cpp
  ${_l}/core/base/file.cpp
  ${_l}/core/base/logging.cpp
  ${_l}/core/base/liblog_symbols.cpp
  ${_l}/core/base/stringprintf.cpp
  ${_l}/core/base/strings.cpp
  ${_l}/core/base/threads.cpp
  ${_l}/core/base/properties.cpp
  ${_l}/core/base/parsebool.cpp
  ${_l}/core/base/utf8.cpp
  ${_l}/core/liblog/logger_write.cpp
  ${_l}/core/liblog/properties.cpp
  ${_l}/src/zip_archive_stream_entry.cc
  ${_l}/core/libziparchive/zip_archive.cc
  ${_l}/src/linker.cpp
  ${_l}/bionic/libdl/libdl.cpp
  ${_l}/bionic/libc/upstream-openbsd/lib/libc/string/strlcpy.c
  ${_l}/bionic/libc/upstream-openbsd/lib/libc/string/strlcat.c
  ${CMAKE_SOURCE_DIR}/src/elf/async_safe_log_windows.cc
  ${CMAKE_SOURCE_DIR}/src/elf/linker_tls_windows.cc
  ${CMAKE_SOURCE_DIR}/src/elf/stack_guard_rewrite.cc
  ${CMAKE_SOURCE_DIR}/src/elf/guest_images.cc
)
set_target_properties(elixir_linker PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_include_directories(elixir_linker BEFORE PRIVATE
  "${CMAKE_SOURCE_DIR}/src/platform/windows/posix_compat/include"
  "${ELIXIR_SYMLINK_ROOT}/third_party/mcpelauncher-linker/include"
  "${_l}/include"
  "${_l}/bionic/libc/async_safe/include"
  "${_l}/core/base/include"
  "${_l}/core/liblog/include"
  "${_l}/core/libcutils/include"
  "${ELIXIR_SYMLINK_ROOT}/third_party/mcpelauncher-linker/core/libcutils/include"
  "${_l}/core/libziparchive/include"
  "${_l}/bionic/linker"
  "${CMAKE_SOURCE_DIR}/src")
target_include_directories(elixir_linker PUBLIC "${_l}/public_include")
target_compile_definitions(elixir_linker PRIVATE
  PATH_MAX=4096 _GNU_SOURCE NOMINMAX
  __LP64__=1 WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_WARNINGS ELIXIR_WINDOWS_LINKER=1)
target_compile_options(elixir_linker PRIVATE
  /EHsc /utf-8 /FIcompat.h /clang:-mno-red-zone /clang:-w
  $<$<CONFIG:Release,RelWithDebInfo>:/O2>)
target_link_libraries(elixir_linker PUBLIC elixir_posix_host elixir_platform_windows elixir_abi zlibstatic)
