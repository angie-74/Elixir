// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_POSIX_COMPAT_H_
#define ELIXIR_POSIX_COMPAT_H_

#if !defined(_WIN32)
#error "elixir_posix_compat.h is only for Windows host builds"
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ELIXIR_POSIX_TYPES_DEFINED
#define ELIXIR_POSIX_TYPES_DEFINED
typedef long long ssize_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long long off64_t;
typedef unsigned int useconds_t;
typedef unsigned int mode_t;
#endif

void* mmap(void* address, size_t length, int prot, int flags, int fd, off64_t offset);
void* mmap64(void* address, size_t length, int prot, int flags, int fd, off64_t offset);
int munmap(void* address, size_t length);
int mprotect(void* address, size_t length, int prot);
int madvise(void* address, size_t length, int advice);

ssize_t pread(int fd, void* buffer, size_t count, off64_t offset);
ssize_t pread64(int fd, void* buffer, size_t count, off64_t offset);
ssize_t pwrite(int fd, const void* buffer, size_t count, off64_t offset);
ssize_t pwrite64(int fd, const void* buffer, size_t count, off64_t offset);
off64_t lseek64(int fd, off64_t offset, int whence);
ssize_t readlink(const char* path, char* buffer, size_t size);
char* realpath(const char* path, char* resolved);
int elixir_posix_fd_path(int fd, char* buffer, size_t size);
int fsync(int fd);
int ftruncate(int fd, off64_t length);
long sysconf(int name);
int getpagesize(void);
pid_t gettid(void);
int usleep(useconds_t microseconds);
unsigned int sleep(unsigned int seconds);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int strcasecmp(const char* left, const char* right);
int strncasecmp(const char* left, const char* right, size_t count);
char* basename(const char* path);
char* dirname(char* path);
const char* getprogname(void);
char* stpcpy(char* destination, const char* source);

#ifdef __cplusplus
}
#endif

#endif
