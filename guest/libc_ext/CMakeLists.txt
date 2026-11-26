# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

set(_ext "${CMAKE_CURRENT_SOURCE_DIR}")

set(ELIXIR_LIBC_EXT_UNWIND_EXPORTS
  _Unwind_Backtrace _Unwind_DeleteException _Unwind_FindEnclosingFunction
  _Unwind_Find_FDE _Unwind_ForcedUnwind _Unwind_GetCFA _Unwind_GetDataRelBase
  _Unwind_GetGR _Unwind_GetIP _Unwind_GetIPInfo _Unwind_GetLanguageSpecificData
  _Unwind_GetRegionStart _Unwind_GetTextRelBase _Unwind_RaiseException
  _Unwind_Resume _Unwind_Resume_or_Rethrow _Unwind_SetGR _Unwind_SetIP
  __register_frame __deregister_frame)
file(GLOB ELIXIR_NDK_LIBUNWIND
  "${ANDROID_TOOLCHAIN_ROOT}/lib/clang/*/lib/linux/x86_64/libunwind.a")
list(LENGTH ELIXIR_NDK_LIBUNWIND _unwind_count)
if(NOT _unwind_count EQUAL 1)
  message(FATAL_ERROR "expected exactly one NDK libunwind.a for x86_64, found: ${ELIXIR_NDK_LIBUNWIND}")
endif()
find_program(ELIXIR_LLVM_OBJCOPY NAMES llvm-objcopy
  HINTS "${ANDROID_TOOLCHAIN_ROOT}/bin" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT ELIXIR_LLVM_OBJCOPY)
  message(FATAL_ERROR "llvm-objcopy not found in ${ANDROID_TOOLCHAIN_ROOT}/bin")
endif()
set(_visibility_args)
foreach(_symbol IN LISTS ELIXIR_LIBC_EXT_UNWIND_EXPORTS)
  list(APPEND _visibility_args "--set-symbol-visibility=${_symbol}=default")
endforeach()
set(ELIXIR_LIBUNWIND_EXPORTED "${CMAKE_CURRENT_BINARY_DIR}/libunwind_exported.a")
add_custom_command(
  OUTPUT "${ELIXIR_LIBUNWIND_EXPORTED}"
  COMMAND "${ELIXIR_LLVM_OBJCOPY}" ${_visibility_args} "${ELIXIR_NDK_LIBUNWIND}" "${ELIXIR_LIBUNWIND_EXPORTED}"
  DEPENDS "${ELIXIR_NDK_LIBUNWIND}"
  COMMENT "Exporting the NDK libunwind.a entry points"
  VERBATIM)
add_custom_target(elixir_libunwind_exported DEPENDS "${ELIXIR_LIBUNWIND_EXPORTED}")

set(ELIXIR_LIBC_EXT_COMMON_FLAGS
  -D_LIBC=1 -Wall -Wextra -Wunused -Wno-char-subscripts -Wno-deprecated-declarations
  -Wno-gcc-compat -Wno-reorder-init-list -Werror -fno-builtin -fPIC -fstack-clash-protection)

set(ELIXIR_LIBC_EXT_FREEBSD
  ${_ext}/upstream-freebsd/lib/libc/gen/glob.c
  ${_ext}/upstream-freebsd/lib/libc/stdlib/getopt_long.c
  ${_ext}/upstream-freebsd/lib/libc/stdlib/hcreate.c
  ${_ext}/upstream-freebsd/lib/libc/stdlib/hcreate_r.c
  ${_ext}/upstream-freebsd/lib/libc/stdlib/hdestroy_r.c
  ${_ext}/upstream-freebsd/lib/libc/stdlib/hsearch_r.c)
set_source_files_properties(${ELIXIR_LIBC_EXT_FREEBSD} PROPERTIES
  COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-parameter;-include;freebsd-compat.h"
  INCLUDE_DIRECTORIES "${_ext}/upstream-freebsd/android/include")
set_property(SOURCE
    ${_ext}/upstream-freebsd/lib/libc/stdlib/hcreate.c
    ${_ext}/upstream-freebsd/lib/libc/stdlib/hcreate_r.c
    ${_ext}/upstream-freebsd/lib/libc/stdlib/hdestroy_r.c
    ${_ext}/upstream-freebsd/lib/libc/stdlib/hsearch_r.c
  APPEND PROPERTY COMPILE_OPTIONS "-D__hsearch=__opaque")

set(ELIXIR_LIBC_EXT_NETBSD
  ${_ext}/upstream-netbsd/lib/libc/inet/nsap_addr.c
  ${_ext}/upstream-netbsd/lib/libc/regex/regcomp.c
  ${_ext}/upstream-netbsd/lib/libc/regex/regerror.c
  ${_ext}/upstream-netbsd/lib/libc/regex/regexec.c
  ${_ext}/upstream-netbsd/lib/libc/regex/regfree.c)
set_source_files_properties(${ELIXIR_LIBC_EXT_NETBSD} PROPERTIES
  COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-parameter;-DPOSIX_MISTAKE;-include;netbsd-compat.h"
  INCLUDE_DIRECTORIES "${_ext}/upstream-netbsd/android/include;${_ext}/upstream-netbsd/lib/libc/include")

set(ELIXIR_LIBC_EXT_OPENBSD
  ${_ext}/upstream-openbsd/lib/libc/gen/err.c
  ${_ext}/upstream-openbsd/lib/libc/gen/errx.c
  ${_ext}/upstream-openbsd/lib/libc/gen/fnmatch.c
  ${_ext}/upstream-openbsd/lib/libc/gen/verr.c
  ${_ext}/upstream-openbsd/lib/libc/gen/verrx.c
  ${_ext}/upstream-openbsd/lib/libc/gen/vwarn.c
  ${_ext}/upstream-openbsd/lib/libc/gen/vwarnx.c
  ${_ext}/upstream-openbsd/lib/libc/gen/warn.c
  ${_ext}/upstream-openbsd/lib/libc/gen/warnx.c
  ${_ext}/upstream-openbsd/lib/libc/net/base64.c
  ${_ext}/upstream-openbsd/lib/libc/net/inet_ntop.c
  ${_ext}/upstream-openbsd/lib/libc/net/res_random.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/getsubopt.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/insque.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/recallocarray.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/remque.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/tfind.c
  ${_ext}/upstream-openbsd/lib/libc/stdlib/tsearch.c
  ${_ext}/bionic/fts.c)
set_source_files_properties(${ELIXIR_LIBC_EXT_OPENBSD} PROPERTIES
  COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-parameter;-include;openbsd-compat.h"
  INCLUDE_DIRECTORIES "${_ext}/private;${_ext}/upstream-openbsd/android/include")
set_property(SOURCE ${_ext}/upstream-openbsd/lib/libc/stdlib/recallocarray.c APPEND PROPERTY
  COMPILE_OPTIONS "-D__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__")

set(ELIXIR_LIBC_EXT_DNS
  ${_ext}/dns/nameser/ns_name.c
  ${_ext}/dns/nameser/ns_netint.c
  ${_ext}/dns/nameser/ns_parse.c
  ${_ext}/dns/nameser/ns_print.c
  ${_ext}/dns/nameser/ns_samedomain.c
  ${_ext}/dns/nameser/ns_ttl.c
  ${_ext}/dns/net/nsdispatch.c
  ${_ext}/dns/resolv/res_cache.c
  ${_ext}/dns/resolv/res_comp.c
  ${_ext}/dns/resolv/res_data.cpp
  ${_ext}/dns/resolv/res_debug.c
  ${_ext}/dns/resolv/res_init.c
  ${_ext}/dns/resolv/res_mkquery.c
  ${_ext}/dns/resolv/res_query.c
  ${_ext}/dns/resolv/res_send.c
  ${_ext}/dns/resolv/res_state.c
  ${_ext}/dns/resolv/res_stats.c
  ${_ext}/upstream-netbsd/lib/libc/isc/ev_streams.c
  ${_ext}/upstream-netbsd/lib/libc/isc/ev_timers.c)
set_source_files_properties(${ELIXIR_LIBC_EXT_DNS} PROPERTIES
  COMPILE_OPTIONS "-DANDROID_CHANGES;-DINET6;-Wno-unused-parameter;-include;netbsd-compat.h"
  INCLUDE_DIRECTORIES "${_ext}/dns/include;${_ext}/private;${_ext}/upstream-netbsd/lib/libc/include;${_ext}/upstream-netbsd/android/include;${_ext}/async_safe/include")
set_property(SOURCE ${_ext}/dns/resolv/res_data.cpp APPEND PROPERTY
  COMPILE_OPTIONS "-fno-exceptions;-fno-rtti")
set_property(SOURCE ${_ext}/dns/resolv/res_send.c APPEND PROPERTY
  COMPILE_OPTIONS "-ftrivial-auto-var-init=zero;-D__connect=connect")

set(ELIXIR_LIBC_EXT_BIONIC
  ${_ext}/bionic/error.cpp
  ${_ext}/bionic/execinfo.cpp
  ${_ext}/bionic/ftw.cpp
  ${_ext}/bionic/lfs64_support.cpp
  ${_ext}/bionic/lockf.cpp
  ${_ext}/bionic/mntent.cpp
  ${_ext}/bionic/netinet_ether.cpp
  ${_ext}/bionic/swab.cpp
  ${_ext}/bionic/tdestroy.cpp
  ${_ext}/bionic/termios.cpp)
set_source_files_properties(${ELIXIR_LIBC_EXT_BIONIC} PROPERTIES
  COMPILE_OPTIONS "--target=x86_64-none-linux-android35;-fno-exceptions;-fno-rtti")

add_library(elixir_libc_ext SHARED
  ${ELIXIR_LIBC_EXT_FREEBSD} ${ELIXIR_LIBC_EXT_NETBSD} ${ELIXIR_LIBC_EXT_OPENBSD}
  ${ELIXIR_LIBC_EXT_DNS} ${ELIXIR_LIBC_EXT_BIONIC})
set_target_properties(elixir_libc_ext PROPERTIES
  OUTPUT_NAME elixir-extension
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  C_STANDARD 99 C_EXTENSIONS ON
  CXX_STANDARD 17 CXX_EXTENSIONS ON
  LINK_DEPENDS "${_ext}/libc_ext.map.txt;${ELIXIR_LIBUNWIND_EXPORTED}")
target_include_directories(elixir_libc_ext PRIVATE "${_ext}")
target_compile_options(elixir_libc_ext PRIVATE ${ELIXIR_LIBC_EXT_COMMON_FLAGS} -O2)
add_dependencies(elixir_libc_ext elixir_libunwind_exported)
target_link_options(elixir_libc_ext PRIVATE
  -nostdlib++ --unwindlib=none
  -Wl,--version-script=${_ext}/libc_ext.map.txt
  -Wl,-Bsymbolic
  -Wl,-soname,libelixir-extension.so
  -Wl,-z,max-page-size=16384 -Wl,--no-undefined)
target_link_libraries(elixir_libc_ext PRIVATE
  -Wl,--whole-archive "${ELIXIR_LIBUNWIND_EXPORTED}" -Wl,--no-whole-archive
  dl)
