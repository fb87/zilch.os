# Module: default_manifest

## Purpose

Fallback `sys_arm64_domain_guest_manifest` definition, used when no guest
package supplies its own via `DOMAIN_GUEST_MANIFEST` (see `user.mk`).

## Responsibilities

- Describe the built-in verification guest's (`src/user/guests/test-arm64`)
  needs: RAM size, initial stack/pstate, and zero device-passthrough
  entries -- that guest never touches the UART, so it has none to declare.
- Nothing else: a guest package that actually drives hardware (see
  `samples/guests/zephyr/manifest.cc`, which forwards its UART via vPL011
  trap-and-emulate) supplies its own manifest instead of this one.

## Invariants

- No exceptions, RTTI, implicit allocation, or hosted C++ runtime dependency.
- Must satisfy `sys::guest_manifest::valid()` (correct magic/version,
  `device_count` within bounds) -- nothing enforces that at compile time,
  only at whatever point `domain-manager` first validates the manifest it
  was linked against.

## Verification

Exercised whenever `domain-manager` is built without a
`DOMAIN_GUEST_MANIFEST` override -- the default for the built-in
verification guest.
