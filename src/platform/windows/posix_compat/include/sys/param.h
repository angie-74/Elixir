// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_SYS_PARAM_H_
#define ELIXIR_POSIX_SYS_PARAM_H_
#include <limits.h>
#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif
#ifndef powerof2
#define powerof2(x) ((((x) - 1) & (x)) == 0)
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#endif
