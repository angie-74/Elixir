# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
cmake_minimum_required(VERSION 3.20)
project(elixir_guest_runtime LANGUAGES C CXX)

if(NOT ANDROID OR NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
  message(FATAL_ERROR "guest/ must be built with the NDK for ANDROID_ABI=x86_64")
endif()

include(FetchContent)
FetchContent_Declare(
  zlib_source
  URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib131.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(zlib_source)
if(NOT zlib_source_POPULATED)
  FetchContent_Populate(zlib_source)
endif()

add_compile_options(
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=guest"
  "-ffile-prefix-map=${zlib_source_SOURCE_DIR}=zlib")
add_link_options(-Wl,--strip-debug)

set(_z "${zlib_source_SOURCE_DIR}")
add_library(z SHARED
  ${_z}/adler32.c ${_z}/compress.c ${_z}/crc32.c ${_z}/deflate.c ${_z}/gzclose.c
  ${_z}/gzlib.c ${_z}/gzread.c ${_z}/gzwrite.c ${_z}/infback.c ${_z}/inffast.c
  ${_z}/inflate.c ${_z}/inftrees.c ${_z}/trees.c ${_z}/uncompr.c ${_z}/zutil.c)
target_include_directories(z PUBLIC "${_z}")
target_compile_definitions(z PRIVATE Z_HAVE_UNISTD_H)
target_compile_options(z PRIVATE -O3 -fPIC -Wno-deprecated-non-prototype)
target_link_options(z PRIVATE -Wl,-soname,libz.so -Wl,-z,max-page-size=16384 -Wl,--no-undefined)
set_target_properties(z PROPERTIES OUTPUT_NAME z)

add_subdirectory(libc_ext)
