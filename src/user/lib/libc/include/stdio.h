#pragma once

#include <stdarg.h>
#include <stddef.h>

extern "C" {

/*
 * Conversions are limited to what this userspace actually formats: %s, %c,
 * %d/%i, %u, %x/%X, %p, %%, with a width and a zero-pad flag. No floating
 * point -- there is no soft-float runtime linked here and nothing to print.
 * An unsupported conversion is emitted literally rather than silently
 * dropped, so a mistake shows up in the output instead of vanishing.
 */
int printf(const char* format, ...) noexcept;
int snprintf(char* buffer, size_t size, const char* format, ...) noexcept;
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments) noexcept;
int dprintf(int descriptor, const char* format, ...) noexcept;

int puts(const char* text) noexcept;
int putchar(int character) noexcept;

/* Reads a line into `buffer`, keeping the newline, and NUL-terminates.
 * Returns null at end of input. */
char* fgets(char* buffer, int size, void* stream) noexcept;

} // extern "C"
