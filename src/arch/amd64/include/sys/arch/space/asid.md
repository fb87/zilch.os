# Module: asid (amd64)

## Purpose

PCID (Process Context ID) allocation for amd64 address spaces: a 64-slot
bitmap with generation-rollover, the amd64 analogue of arm64's ASID
allocator of the same interface shape (`handle`, `allocate`/`refresh`/
`release`).

## Responsibilities

- Allocate and release PCID values 1-63 (0 is reserved for the kernel),
  tracking a generation counter per slot so a stale `handle` from before a
  rollover is detected rather than silently reused.
- `invalidate_all()` uses `INVPCID` type 2 (all-context) when
  `CPUID.(EAX=07H,ECX=0):EBX` bit 10 reports it available, and falls back
  to a full CR3 round-trip reload otherwise -- the one point where this
  differs structurally from arm64's ASID allocator, which has no
  hardware-support query to make.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Capacity fixed at 64 to match arm64's ASID allocator, keeping the two
  architectures' address-space code structurally comparable.

## Verification

The colocated `asid.tt` file is reserved for module-level tests. Not yet
exercised under actual amd64 execution (see `entry.md`'s note on this
platform's QEMU multiboot limitation); verified only by compilation and by
`address_space.hh`'s use of this allocator during `initialize()`.
