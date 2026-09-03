#pragma once

/*
 * The minimum the rest of this libc needs to describe itself.
 *
 * Userspace builds -ffreestanding -nostdinc++ against a bare-metal target,
 * so there is no system libc to take these from. They are spelled the C way
 * rather than reusing sys/types.hh's names because the point of this
 * directory is that ordinary C code compiles against it unchanged.
 */

using size_t = unsigned long;
using ssize_t = long;
using ptrdiff_t = long;

static_assert(sizeof(size_t) == 8U, "LP64 is assumed throughout this libc");

#ifndef NULL
#define NULL nullptr
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)
