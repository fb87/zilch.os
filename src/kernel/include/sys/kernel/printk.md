# Module: `sys::printk`

## Purpose

Provides synchronous, allocation-free kernel console output for early boot and diagnostics.

## Interface

- `printk(format, ...)`
- `vprintk(format, va_list)`
- `pr_err(format, ...)`
- `pr_warn(format, ...)`
- `pr_info(format, ...)`
- `pr_debug(format, ...)`

The complete formatter and console writer are implemented in `printk.hh`.
There is no separate `vsprintf`, `vsnprintf`, formatter source, C++ template,
heap allocation, lock, or intermediate output buffer.

## `va_list` rule

`printk()` owns `va_start()` and `va_end()`. `vprintk()` consumes the supplied
`va_list` directly. All `va_arg()` operations occur inside `vprintk()`; the
argument list is not passed by C++ reference to helper functions.

## Supported conversions

- `%c`
- `%s`
- `%d`, `%i`
- `%u`
- `%x`, `%X`
- `%p`
- `%%`

Integer conversions support `l`, `ll`, and `z` length modifiers.

## Invariants

- The console backend must be initialized before output.
- Callers must pass arguments matching the conversion and length modifier.
- Output is synchronous and unbuffered.
- Output may interleave after SMP is enabled; serialization is future work.
- Formatting must not allocate memory or depend on the C++ standard library.

## Verification

`printk.tt` is the colocated test translation unit. Target runtime validation
must exercise all supported conversions using a capture-console backend and an
early-boot smoke test on both ARM64 and AMD64.
