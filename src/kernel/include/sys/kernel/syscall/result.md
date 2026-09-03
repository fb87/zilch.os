# Module: result

## Purpose

`set_control_result()`: the one place that converts a kernel `error_t` into
the raw `word_t` a control-syscall reply carries, so every `control_operation`
handler in `syscall/control.hh` reports its outcome the same way.

## Responsibilities

- Encode `error_t` as a negated `s64` (matching every ABI success/failure
  convention this codebase uses: 0 for success, a negative value otherwise)
  and hand it to `arch::syscall::set_result()`, keeping the encoding choice
  in exactly one place rather than repeated at every call site.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## Verification

Exercised by every control-plane test that checks a syscall's returned
status, via `dispatch_control()`'s single call site in `syscall/control.hh`.
