// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_MMAN_H_
#define ELIXIR_POSIX_SYS_MMAN_H_
#include <sys/types.h>
#include "elixir_posix_compat.h"

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_FAILED ((void*)-1)
#define MADV_NORMAL 0
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#endif
