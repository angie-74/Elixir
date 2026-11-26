// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_RESOURCE_H_
#define ELIXIR_POSIX_SYS_RESOURCE_H_
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long long rlim_t;
struct rlimit {
  rlim_t rlim_cur;
  rlim_t rlim_max;
};
#define RLIMIT_NOFILE 7
#define RLIM_INFINITY (~0ull)
static inline int getrlimit(int resource, struct rlimit* limit) {
  limit->rlim_cur = resource == RLIMIT_NOFILE ? 8192 : RLIM_INFINITY;
  limit->rlim_max = limit->rlim_cur;
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif
