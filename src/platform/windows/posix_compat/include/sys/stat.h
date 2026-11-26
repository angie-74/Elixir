// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_STAT_H_
#define ELIXIR_POSIX_SYS_STAT_H_
#include <sys/types.h>
#include_next <sys/stat.h>
#include <direct.h>

#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#ifndef S_IFREG
#define S_IFREG _S_IFREG
#endif
#ifndef S_IFLNK
#define S_IFLNK 0xA000
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) 0
#define S_ISCHR(m) (((m) & S_IFMT) == _S_IFCHR)
#define S_ISFIFO(m) 0
#define S_ISSOCK(m) 0
#define S_ISBLK(m) 0
#endif
#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007
#endif

#ifdef __cplusplus
static inline int mkdir(const char* path, unsigned mode) {
  (void)mode;
  return _mkdir(path);
}
#endif
#endif
