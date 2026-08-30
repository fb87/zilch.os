---
module: sys.user.guest_manifest
---

# Guest manifest

`sys::guest_manifest::manifest` is the data-driven description of what a guest
package needs from the domain-manager: RAM size, boot stack/pstate, and a list
of passthrough MMIO devices (each with an optional physical IRQ to forward into
the guest's virtual GIC). It replaces device- and guest-specific C++ constants
that used to be hardcoded in the domain-manager and root task.

A guest package (e.g. `samples/guests/zephyr/`) supplies its own small `.cc`
file defining `sys_arm64_domain_guest_manifest` (an ordinary const struct, no
binary packing or incbin involved) via the `DOMAIN_GUEST_MANIFEST` build
variable, compiled into both the domain-manager and root (`init`) binaries
alongside `DOMAIN_GUEST_ELF`. When a guest package supplies none,
`default_manifest.cc` is used, matching the built-in test guest's needs. The
domain-manager and root task read this struct at runtime instead of assuming
any specific device or guest.
