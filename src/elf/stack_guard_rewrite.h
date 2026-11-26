// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ELF_STACK_GUARD_REWRITE_H_
#define ELIXIR_ELF_STACK_GUARD_REWRITE_H_

#include <cstddef>
#include <cstdint>

namespace elixir::elf {

struct RewriteStats {
  std::size_t rewritten = 0;
  std::size_t skipped_out_of_range = 0;
};

RewriteStats RewriteStackGuardLoads(std::uint8_t* code, std::size_t length, std::uintptr_t image_begin,
                                    std::uintptr_t image_end);

std::size_t TotalStackGuardRewrites();

bool StackGuardRewriteEnabled();
void SetStackGuardRewriteEnabled(bool enabled);

}

#endif
