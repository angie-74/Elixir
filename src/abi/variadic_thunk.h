// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ABI_VARIADIC_THUNK_H_
#define ELIXIR_ABI_VARIADIC_THUNK_H_

#include "abi/sysv_abi.h"

#define ELIXIR_VA_STRINGIFY_(x) #x
#define ELIXIR_VA_STRINGIFY(x) ELIXIR_VA_STRINGIFY_(x)

#define ELIXIR_VA_REG_1 "rsi"
#define ELIXIR_VA_REG_2 "rdx"
#define ELIXIR_VA_REG_3 "rcx"
#define ELIXIR_VA_REG_4 "r8"
#define ELIXIR_VA_REG_5 "r9"

#define ELIXIR_SYSV_VARIADIC_THUNK(thunk_name, impl_symbol, fixed_gp)          \
  extern "C" ELIXIR_SYSV ELIXIR_NAKED void thunk_name() {                     \
    __asm__ volatile(                                                          \
        "subq $216, %rsp\n\t"                                                  \
        "movq %rdi, 0(%rsp)\n\t"                                               \
        "movq %rsi, 8(%rsp)\n\t"                                               \
        "movq %rdx, 16(%rsp)\n\t"                                              \
        "movq %rcx, 24(%rsp)\n\t"                                              \
        "movq %r8, 32(%rsp)\n\t"                                               \
        "movq %r9, 40(%rsp)\n\t"                                               \
        "movaps %xmm0, 48(%rsp)\n\t"                                           \
        "movaps %xmm1, 64(%rsp)\n\t"                                           \
        "movaps %xmm2, 80(%rsp)\n\t"                                           \
        "movaps %xmm3, 96(%rsp)\n\t"                                           \
        "movaps %xmm4, 112(%rsp)\n\t"                                          \
        "movaps %xmm5, 128(%rsp)\n\t"                                          \
        "movaps %xmm6, 144(%rsp)\n\t"                                          \
        "movaps %xmm7, 160(%rsp)\n\t"                                          \
        "movl $" ELIXIR_VA_STRINGIFY(fixed_gp) "*8, 176(%rsp)\n\t"            \
        "movl $48, 180(%rsp)\n\t"                                              \
        "leaq 224(%rsp), %rax\n\t"                                             \
        "movq %rax, 184(%rsp)\n\t"                                             \
        "movq %rsp, 192(%rsp)\n\t"                                             \
        "leaq 176(%rsp), %" ELIXIR_VA_REG_##fixed_gp "\n\t"                    \
        "callq " #impl_symbol "\n\t"                                           \
        "addq $216, %rsp\n\t"                                                  \
        "retq\n\t");                                                           \
  }

#endif
