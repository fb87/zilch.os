# Module: gdt

## Purpose

Extends `start.S`'s minimal 3-entry boot GDT with user code/data segments
and a TSS, then loads it -- the prerequisite `sys::arch::exception`'s real
IDT needs, since every interrupt gate targets this file's kernel code
selector and, on a real ring3->ring0 transition, the CPU consults the TSS
this file installs for which stack to switch to.

## Responsibilities

- Build a 9-slot GDT: null, kernel code (0x08) and kernel data (0x10) kept
  byte-identical to `start.S`'s original entries at those indices, two
  unused slots, user code (0x28, selector-with-RPL3 0x2b) and user data
  (0x30, selector-with-RPL3 0x33) matching what `thread/context.hh` and
  `thread/entry.hh` already hardcode, and a 16-byte TSS descriptor.
- Encode and install one `task_state` (TSS) with `rsp0` pointing at the
  same boot stack `start.S` already set up, and `ltr` it.
- Load the extended table with `lgdt`, reload the data segment registers,
  and load the TSS -- deliberately without reloading `%cs` (see below).

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Does not reload `%cs` via a far jump after `lgdt`. Indices 1 and 2
  (kernel code/data) are byte-identical to what `start.S`'s original boot
  GDT already installed at those same indices -- x86_64 caches a loaded
  segment's descriptor state in hidden fields not re-validated against a
  new GDT until that register is explicitly reloaded, so this cannot
  invalidate the CS already in use. This is a real simplification, not an
  oversight: it removes the highest-risk part of this file (a long-mode
  `retf`/`iretq` CS-reload sequence) in an environment where the result
  cannot be executed to catch a mistake.
- `rsp0` reuses `start.S`'s existing boot stack rather than a dedicated
  interrupt stack. A production kernel would want these separate,
  especially per-CPU; this is a documented limitation, not an assumption
  made silently -- correct for "survive an interrupt at all," which is the
  actual goal here.

## Verification

Verified by compilation, symbol survival under `--gc-sections` (this table
and the TSS were unreferenced dead code before `sys::arch::exception`
started reading them), and disassembly review cross-checking the compiled
`lgdt`/`ltr` sequence and the TSS descriptor's encoded bytes against this
file's intended values byte-for-byte. NOT verified by execution: QEMU's
multiboot loader only accepts a 32-bit kernel (see `tools/run/run.sh`), so
amd64 has not booted in this environment.
