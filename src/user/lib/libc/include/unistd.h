#pragma once

#include <stddef.h>

inline constexpr int STDIN_FILENO = 0;
inline constexpr int STDOUT_FILENO = 1;
inline constexpr int STDERR_FILENO = 2;

/* Open flags. Only the access modes and the creation flags a shell actually
 * uses for redirection are defined; the VFS rejects anything else. */
inline constexpr int O_RDONLY = 0;
inline constexpr int O_WRONLY = 1;
inline constexpr int O_RDWR = 2;
inline constexpr int O_CREAT = 0x40;
inline constexpr int O_TRUNC = 0x200;
inline constexpr int O_APPEND = 0x400;

extern "C" {

ssize_t read(int descriptor, void* buffer, size_t count) noexcept;
ssize_t write(int descriptor, const void* buffer, size_t count) noexcept;
int close(int descriptor) noexcept;
int open(const char* path, int flags) noexcept;
int dup2(int from, int to) noexcept;

/*
 * fork returns 0 in the child and the child's identifier in the parent, as
 * POSIX requires. That identifier doubles as the handle waitpid takes --
 * see io.cc for why it is not the kernel's thread id.
 */
int fork() noexcept;
int execv(const char* path, char* const argv[]) noexcept;
int waitpid(int pid, int* status, int options) noexcept;
[[noreturn]] void _exit(int status) noexcept;

} // extern "C"
