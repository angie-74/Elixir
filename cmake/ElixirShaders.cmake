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

set(ELIXIR_SHADER_SDK "vulkan-sdk-1.4.363.0")
FetchContent_Declare(
  elixir_spirv_headers
  URL https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${ELIXIR_SHADER_SDK}.tar.gz
  URL_HASH SHA256=5b3a0d099f5c7649e29c0938476c462e71d66ad89a0bde42fa1ba3eb476956c9
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(
  elixir_spirv_tools
  URL https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${ELIXIR_SHADER_SDK}.tar.gz
  URL_HASH SHA256=e6c83a215538fbfd265ccf398d3877985055df1273dbed73912bd05307664688
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(
  elixir_glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/${ELIXIR_SHADER_SDK}.tar.gz
  URL_HASH SHA256=cda24ec765e6d2845d106b7caa593f34127195a88524cf87821ff0088fb6edec
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF)
set(SPIRV_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
set(SPIRV_TOOLS_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
if(DEFINED BUILD_TESTING AND NOT BUILD_TESTING)
  foreach(_elixir_shader_dep elixir_spirv_headers elixir_spirv_tools elixir_glslang)
    string(TOUPPER "${_elixir_shader_dep}" _elixir_shader_dep_upper)
    if(FETCHCONTENT_SOURCE_DIR_${_elixir_shader_dep_upper})
      file(TO_CMAKE_PATH "${FETCHCONTENT_SOURCE_DIR_${_elixir_shader_dep_upper}}" _elixir_shader_dep_dir)
    else()
      set(_elixir_shader_dep_dir "${FETCHCONTENT_BASE_DIR}/${_elixir_shader_dep}-src")
    endif()
    add_compile_options(
      "$<$<COMPILE_LANGUAGE:C,CXX>:/clang:-fmacro-prefix-map=${_elixir_shader_dep_dir}=${_elixir_shader_dep}>")
  endforeach()
endif()
FetchContent_MakeAvailable(elixir_spirv_headers)
FetchContent_MakeAvailable(elixir_spirv_tools elixir_glslang)

set(ELIXIR_SHADERS_SRC "${CMAKE_SOURCE_DIR}/src/shaders")
add_library(elixir_shaders STATIC
  "${ELIXIR_SHADERS_SRC}/shader_text.cc"
  "${ELIXIR_SHADERS_SRC}/shader_interface.cc"
  "${ELIXIR_SHADERS_SRC}/shader_compiler.cc"
  "${ELIXIR_SHADERS_SRC}/shader_library.cc"
)
elixir_target(elixir_shaders)
target_include_directories(elixir_shaders PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(elixir_shaders
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE glslang glslang-default-resource-limits SPIRV-Tools-opt SPIRV-Tools-static)

add_library(elixir_shader_chain STATIC
  "${ELIXIR_SHADERS_SRC}/builtin_shaders.cc"
  "${ELIXIR_SHADERS_SRC}/shader_chain.cc"
)
elixir_target(elixir_shader_chain)
target_link_libraries(elixir_shader_chain PUBLIC elixir_shaders elixir_vulkan)

if(BUILD_TESTING)
  elixir_gtest(elixir_shaders_test
    "${CMAKE_SOURCE_DIR}/tests/windows/shaders/shader_compiler_test.cc")
  target_link_libraries(elixir_shaders_test PRIVATE elixir_shaders)

  elixir_gtest(elixir_shader_chain_test
    "${CMAKE_SOURCE_DIR}/tests/windows/shaders/shader_chain_test.cc")
  target_link_libraries(elixir_shader_chain_test PRIVATE elixir_shader_chain user32)
  target_compile_definitions(elixir_shader_chain_test PRIVATE
    ELIXIR_SHADER_ASSETS="${CMAKE_SOURCE_DIR}/assets/shaders")
endif()
