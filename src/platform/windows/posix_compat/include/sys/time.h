// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_TIME_H_
#define ELIXIR_POSIX_SYS_TIME_H_
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
  long tv_sec;
  long tv_usec;
};
#endif
struct timezone;
int gettimeofday(struct timeval* tv, struct timezone* tz);
#ifdef __cplusplus
}
#endif
#endif
