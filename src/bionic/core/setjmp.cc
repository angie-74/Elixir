// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cstdint>

#include "abi/sysv_abi.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

namespace {

constexpr std::uint64_t kSetjmpMagic = 0x454c584a4d500000ull;
constexpr std::uint64_t kSetjmpSavedMask = 1;

}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED void elixir_core_setjmp_record(std::uint64_t* env, int save_mask) noexcept {
  if (save_mask != 0) {
    env[9] = elixir::bionic::core::CurrentSignalMask();
    env[8] = kSetjmpMagic | kSetjmpSavedMask;
  } else {
    env[9] = 0;
    env[8] = kSetjmpMagic;
  }
}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED void elixir_core_longjmp_prepare(std::uint64_t* env) noexcept {
  if ((env[8] & ~kSetjmpSavedMask) != kSetjmpMagic) {
    elixir::bionic::HostLog(7, "libc", "longjmp botch: jmp_buf was not initialized by setjmp");
    elixir::bionic::core::TerminateForSignal(elixir::bionic::guest::kSIGABRT);
  }
  if ((env[8] & kSetjmpSavedMask) != 0) elixir::bionic::core::SetSignalMask(env[9]);
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED int elixir_bionic_sigsetjmp(void*, int) {
  __asm__ volatile(
      "movq %rbx, 0(%rdi)\n\t"
      "movq %rbp, 8(%rdi)\n\t"
      "movq %r12, 16(%rdi)\n\t"
      "movq %r13, 24(%rdi)\n\t"
      "movq %r14, 32(%rdi)\n\t"
      "movq %r15, 40(%rdi)\n\t"
      "leaq 8(%rsp), %rax\n\t"
      "movq %rax, 48(%rdi)\n\t"
      "movq (%rsp), %rax\n\t"
      "movq %rax, 56(%rdi)\n\t"
      "subq $8, %rsp\n\t"
      "callq elixir_core_setjmp_record\n\t"
      "addq $8, %rsp\n\t"
      "xorl %eax, %eax\n\t"
      "retq\n\t");
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED int elixir_bionic_setjmp(void*) {
  __asm__ volatile(
      "movl $1, %esi\n\t"
      "jmp elixir_bionic_sigsetjmp\n\t");
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED int elixir_bionic__setjmp(void*) {
  __asm__ volatile(
      "xorl %esi, %esi\n\t"
      "jmp elixir_bionic_sigsetjmp\n\t");
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED void elixir_bionic_siglongjmp(void*, int) {
  __asm__ volatile(
      "pushq %rdi\n\t"
      "pushq %rsi\n\t"
      "subq $8, %rsp\n\t"
      "callq elixir_core_longjmp_prepare\n\t"
      "addq $8, %rsp\n\t"
      "popq %rsi\n\t"
      "popq %rdi\n\t"
      "movl %esi, %eax\n\t"
      "testl %eax, %eax\n\t"
      "jnz 1f\n\t"
      "incl %eax\n\t"
      "1:\n\t"
      "movq 0(%rdi), %rbx\n\t"
      "movq 8(%rdi), %rbp\n\t"
      "movq 16(%rdi), %r12\n\t"
      "movq 24(%rdi), %r13\n\t"
      "movq 32(%rdi), %r14\n\t"
      "movq 40(%rdi), %r15\n\t"
      "movq 48(%rdi), %rsp\n\t"
      "jmpq *56(%rdi)\n\t");
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED void elixir_bionic_longjmp(void*, int) {
  __asm__ volatile("jmp elixir_bionic_siglongjmp\n\t");
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED void elixir_bionic__longjmp(void*, int) {
  __asm__ volatile("jmp elixir_bionic_siglongjmp\n\t");
}

namespace elixir::bionic::core {

void RegisterSetjmpExports(ExportTable& table) {
  table.Add(kLibc, "setjmp", reinterpret_cast<void*>(&elixir_bionic_setjmp));
  table.Add(kLibc, "_setjmp", reinterpret_cast<void*>(&elixir_bionic__setjmp));
  table.Add(kLibc, "sigsetjmp", reinterpret_cast<void*>(&elixir_bionic_sigsetjmp));
  table.Add(kLibc, "longjmp", reinterpret_cast<void*>(&elixir_bionic_longjmp));
  table.Add(kLibc, "_longjmp", reinterpret_cast<void*>(&elixir_bionic__longjmp));
  table.Add(kLibc, "siglongjmp", reinterpret_cast<void*>(&elixir_bionic_siglongjmp));
}

}
