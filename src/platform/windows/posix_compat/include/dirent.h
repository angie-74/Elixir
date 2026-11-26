// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_DIRENT_H_
#define ELIXIR_POSIX_DIRENT_H_
#include <sys/types.h>
#include "elixir_posix_compat.h"

#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8

#ifdef __cplusplus
extern "C" {
#endif
struct dirent {
  unsigned long long d_ino;
  unsigned char d_type;
  char d_name[1024];
};
typedef struct elixir_posix_dir DIR;
DIR* opendir(const char* path);
DIR* fdopendir(int fd);
struct dirent* readdir(DIR* dir);
int closedir(DIR* dir);
void rewinddir(DIR* dir);
#ifdef __cplusplus
}
#endif
#endif
