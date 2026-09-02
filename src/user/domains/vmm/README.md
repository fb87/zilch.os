# vmm

Guest-domain machinery shared by the VMM: the parts of hosting a guest that
are *vocabulary* rather than *authority*.

Headers live under `include/sys/vmm/` and are on the userspace include path
(`src/user/user.mk`'s `USER_CPPFLAGS`), so a server reaches them as
`#include <sys/vmm/vpl011.hh>`.

| Header      | Contents                                                        |
|-------------|-----------------------------------------------------------------|
| `elf.hh`    | ELF64 guest-image layout and header validation                   |
| `vpl011.hh` | Virtual PL011 UART: register map, state, stage-2 abort decoding   |

## The split

The dividing line is **capabilities**, not subject matter. Everything here is
data and pure functions; nothing in this directory invokes a syscall or holds
a capability.

`src/user/servers/domain` keeps the parts that wield authority — forwarding
guest characters to console-server, injecting the guest's IRQ, advancing the
guest PC, mapping guest frames, and the `domain.launch/configure/run/destroy`
control-plane dispatch. Those need capabilities that server holds, and moving
them here would either drag the capabilities along or turn the split into
parameter-passing for its own sake.

So `vpl011.hh` models the device; the server drives it.

## Two things worth knowing

**`elf.hh` is not the kernel's ELF loader.** Each arch has its own
`sys/arch/space/elf64.hh` for loading zilch's *own* userspace binaries in
kernel context. A guest image is untrusted input parsed by an unprivileged
userspace process, and shares only the on-disk layout. Keeping them separate
is deliberate.

**`vpl011.hh`'s qualification bit layout is arm64-specific.** The register
model is portable, but decoding which register a trapped access used comes
from the stage-2 abort syndrome (matching
`src/arch/arm64/include/sys/arch/hypervisor.hh`'s `mmio_qualification()`). A
VMX host decodes an exit qualification with an entirely different layout, and
an x86 guest would expect a 16550A rather than a PL011 anyway.

## Reachability

The guest execution path is **not reachable in either default build
profile**, which is worth knowing before changing anything here:

- `release` runs `supervise()` (`CONFIG_SELFTEST=0`) but compiles the guest
  loop out (`CONFIG_GUEST_EMBEDDED_IMAGE=0`).
- `debug` sets `CONFIG_GUEST_EMBEDDED_IMAGE=1`, but `CONFIG_SELFTEST=1` means
  `init` runs the certification suite instead of `supervise()`, so
  `run_embedded_guest_loop()` is never called.

They are mutually exclusive by construction: `GUEST_TEST_ARM64` — which is
what supplies the built-in guest ELF — `depends on SELFTEST`. Guest behaviour
is covered by the hypervisor model tests in the certification suite rather
than by executing a guest under `supervise()`.

Consequently this extraction was verified by comparing build artifacts rather
than by running a guest: every user binary, `domain-manager` included, is
bit-identical before and after. For a pure code move that is a stronger
guarantee than a runtime test would have given, and it was the only one
available.
