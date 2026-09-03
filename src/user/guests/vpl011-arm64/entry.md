# Module: entry (vpl011-arm64 guest)

## Purpose

Minimal ARM64 guest that exercises the domain manager's *production*
guest-hosting path -- stage-2 trap-and-emulate through
`sys::vmm::vpl011` -- rather than the HVC contract
`src/user/guests/test-arm64` speaks to the certification harness. See
`README.md` in this directory for the fuller story: why this guest exists
alongside `test-arm64`, and what making the guest-execution path reachable
at all uncovered.

## Responsibilities

- Write a fixed string to `0x09000000` (the vPL011 data register) one byte
  at a time via ordinary stores, then park in `wfe` forever.
- Use no hypercall of any kind: every store is meant to fault to stage-2
  and be emulated, not serviced cooperatively.

## Invariants

- No hosted C++ runtime; this is not even C++, it is hand-written
  AArch64 assembly linked at guest-physical address 0.
- Must not include private kernel headers -- this is guest software, built
  and executed as an untrusted guest image, not kernel or architecture boot
  code.

## Verification

`configs/guest_defconfig` embeds this as the domain manager's guest image;
`tools/verification/smoke.sh` (`make smoke`) boots it and asserts on
`guest alive via vpl011` reaching the real UART through console-server.
