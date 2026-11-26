# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
add_library(elixir_fflags STATIC
  src/fflags/fflags.cc
  src/fflags/presets.cc
  src/fflags/editor.cc
)
elixir_target(elixir_fflags)
target_include_directories(elixir_fflags PUBLIC "${CMAKE_SOURCE_DIR}/src")

if(BUILD_TESTING)
  elixir_gtest(elixir_fflags_test
    tests/windows/fflags/fflags_test.cc
    tests/windows/fflags/editor_test.cc)
  target_link_libraries(elixir_fflags_test PRIVATE elixir_fflags)
  target_compile_definitions(elixir_fflags_test PRIVATE
    ELIXIR_SOURCE_DIR_DEFINE="${CMAKE_SOURCE_DIR}")
endif()
