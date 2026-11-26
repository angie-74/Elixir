// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_DLFCN_H_
#define ELIXIR_POSIX_DLFCN_H_

#define RTLD_LOCAL 0
#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_NOLOAD 0x00004
#define RTLD_GLOBAL 0x00100
#define RTLD_NODELETE 0x01000
#define RTLD_DEFAULT ((void*)0)
#define RTLD_NEXT ((void*)~(unsigned long long)0)

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  const char* dli_fname;
  void* dli_fbase;
  const char* dli_sname;
  void* dli_saddr;
} Dl_info;
#ifdef __cplusplus
}
#endif
#endif
