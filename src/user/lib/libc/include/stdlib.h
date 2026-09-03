#pragma once

#include <stddef.h>

extern "C" {

/*
 * The heap is grown by creating frames and mapping them above the stack;
 * see malloc.cc. It is bounded by the task's page quota, so allocation
 * failure is a normal outcome here rather than a sign of a broken system,
 * and every caller is expected to check for null.
 */
void* malloc(size_t size) noexcept;
void free(void* pointer) noexcept;
void* calloc(size_t count, size_t size) noexcept;
void* realloc(void* pointer, size_t size) noexcept;

[[noreturn]] void exit(int status) noexcept;
[[noreturn]] void abort() noexcept;

int atoi(const char* text) noexcept;
long strtol(const char* text, char** end, int base) noexcept;
char* getenv(const char* name) noexcept;

} // extern "C"
