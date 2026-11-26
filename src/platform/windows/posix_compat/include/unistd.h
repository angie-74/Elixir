// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_UNISTD_H_
#define ELIXIR_POSIX_UNISTD_H_

#include <direct.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "elixir_posix_compat.h"

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#ifndef F_OK
#define F_OK 0
#define W_OK 2
#define R_OK 4
#endif
#ifndef X_OK
#define X_OK 0
#endif

#define _SC_CLK_TCK 0x0006
#define _SC_PAGESIZE 0x0027
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 0x0060
#define _SC_NPROCESSORS_ONLN 0x0061
#define _SC_PHYS_PAGES 0x0062

#endif
