// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_FTW_H_
#define ELIXIR_POSIX_FTW_H_
#include <sys/stat.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FTW_F 0
#define FTW_D 1
#define FTW_DNR 2
#define FTW_NS 3
#define FTW_SL 4
#define FTW_DP 5
#define FTW_SLN 6
#define FTW_PHYS 1
#define FTW_MOUNT 2
#define FTW_CHDIR 4
#define FTW_DEPTH 8
struct FTW {
  int base;
  int level;
};
int nftw(const char* path, int (*callback)(const char*, const struct stat*, int, struct FTW*), int fds, int flags);
#ifdef __cplusplus
}
#endif
#endif
