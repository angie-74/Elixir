// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_UIO_H_
#define ELIXIR_POSIX_SYS_UIO_H_
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct iovec {
  void* iov_base;
  size_t iov_len;
};
ssize_t writev(int fd, const struct iovec* vector, int count);
#ifdef __cplusplus
}
#endif
#endif
