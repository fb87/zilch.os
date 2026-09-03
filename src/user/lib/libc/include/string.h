#pragma once

#include <stddef.h>

/*
 * memcpy and memset are not optional conveniences here: the compiler emits
 * calls to them for aggregate copies and value-initialised arrays even under
 * -fno-builtin, so any translation unit that copies a struct needs them to
 * exist. The kernel side of the tree works around that by hand-writing
 * loops; userspace gets real definitions instead.
 */
extern "C" {

void* memcpy(void* destination, const void* source, size_t count) noexcept;
void* memmove(void* destination, const void* source, size_t count) noexcept;
void* memset(void* destination, int value, size_t count) noexcept;
int memcmp(const void* left, const void* right, size_t count) noexcept;
void* memchr(const void* haystack, int needle, size_t count) noexcept;

size_t strlen(const char* text) noexcept;
size_t strnlen(const char* text, size_t limit) noexcept;
int strcmp(const char* left, const char* right) noexcept;
int strncmp(const char* left, const char* right, size_t limit) noexcept;
char* strcpy(char* destination, const char* source) noexcept;
char* strncpy(char* destination, const char* source, size_t limit) noexcept;
char* strcat(char* destination, const char* source) noexcept;
char* strchr(const char* text, int character) noexcept;
char* strrchr(const char* text, int character) noexcept;
char* strstr(const char* haystack, const char* needle) noexcept;

} // extern "C"
