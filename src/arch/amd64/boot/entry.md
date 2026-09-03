# Module: entry (amd64 boot)

## Purpose

`sys_amd64_enter_user`: the one-way transition from kernel context into
ring-3 userspace, via `iretq`. Hand-written, unlike `vectors_low.S`/
`vectors_high.S` alongside it -- kept as its own file specifically so it
survives untouched if `tools/kernel/gen_vectors.py` ever regenerates those.

## Responsibilities

- Given a pointer to a `frame_t` in `%rdi`, load the saved stack pointer,
  push the five-word `iretq` frame (SS, RSP, RFLAGS, CS, RIP) from the
  saved context, restore every general-purpose register from the frame,
  and execute `iretq`.
- Nothing else: this function does not return, and does not validate the
  frame it is given -- `sys::arch::thread::valid_user()` is the caller-side
  check for that, before `enter_user()` is ever invoked.

## Invariants

- The field offsets read here (`160`=stack_pointer, `168`=ss, `152`=status,
  `144`=cs, `136`=instruction_pointer, `0..112`=general-purpose registers)
  must match `sys::arch::exception::frame_t`'s layout exactly. That
  struct's `static_assert`s on `sizeof`/offsets are the actual check; this
  file has no compile-time link to them and would silently misload
  registers if the two ever drifted apart.
- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.

## Verification

Reachable only once amd64 userspace execution is exercised end to end
(Phase 7/8 territory) -- not yet run under QEMU on this platform (see
`tools/run/run.sh`'s amd64 branch: QEMU's multiboot loader only accepts a
32-bit kernel, so this codebase's 64-bit amd64 image has not been booted at
all yet, only compiled and linked).
