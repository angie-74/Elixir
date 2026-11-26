// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_FCNTL_H_
#define ELIXIR_POSIX_FCNTL_H_
#include_next <fcntl.h>
#include <io.h>
#include "elixir_posix_compat.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC _O_NOINHERIT
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif
#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef F_GETFD
#define F_GETFD 1
#define F_SETFD 2
#define FD_CLOEXEC 1
#endif
#endif
