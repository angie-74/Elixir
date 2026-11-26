// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_PTHREAD_H_
#define ELIXIR_POSIX_PTHREAD_H_

#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "elixir_posix_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void* srw;
  unsigned long owner;
  unsigned count;
  int type;
} pthread_mutex_t;

typedef struct {
  int type;
} pthread_mutexattr_t;

typedef struct {
  void* init_once;
} pthread_once_t;

typedef unsigned long pthread_t;

typedef struct {
  void* srw;
  unsigned long writer;
} pthread_rwlock_t;
typedef struct {
  int unused;
} pthread_rwlockattr_t;
#define PTHREAD_RWLOCK_INITIALIZER {0, 0}

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_INITIALIZER {0, 0, 0, PTHREAD_MUTEX_NORMAL}
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP {0, 0, 0, PTHREAD_MUTEX_RECURSIVE}
#define PTHREAD_ONCE_INIT {0}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_once(pthread_once_t* once, void (*routine)(void));
pthread_t pthread_self(void);
int pthread_rwlock_init(pthread_rwlock_t* lock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* lock);
int pthread_rwlock_rdlock(pthread_rwlock_t* lock);
int pthread_rwlock_wrlock(pthread_rwlock_t* lock);
int pthread_rwlock_unlock(pthread_rwlock_t* lock);

#ifdef __cplusplus
}
#endif

#endif
