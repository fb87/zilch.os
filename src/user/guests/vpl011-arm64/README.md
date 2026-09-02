# ARM64 vPL011 guest

A minimal guest that exercises the **production** guest-hosting path:
stage-2 trap-and-emulate through the domain manager's virtual PL011.

It writes a string to `0x09000000` (the vPL011 data register) with ordinary
stores and then parks in `WFE`. Every store faults to stage-2, is emulated by
`sys::vmm::vpl011`, forwarded to console-server, and printed on the real UART
by the serial driver — so seeing its output proves that whole chain.

## Why this exists alongside `guests/test-arm64`

`test-arm64` speaks a guest-visible **HVC contract** to the certification
harness. It cannot run under the domain manager, which hosts guests by
trapping MMIO rather than by servicing hypercalls — which is exactly why it
`depends on SELFTEST`.

That dependency created a deadlock: the guest execution path needs
`CONFIG_GUEST_EMBEDDED_IMAGE` *and* `supervise()` (`CONFIG_SELFTEST=0`), but
the only built-in guest image required `SELFTEST=y`. So the path was
unreachable in every profile.

This guest uses no hypercalls, so it carries no `SELFTEST` dependency and
breaks the deadlock. `configs/guest_defconfig` is the profile that combines
them:

```
make ARCH=arm64 PLATFORM=qemu-arm64-virt BUILD_VARIANT=development \
     KCONFIG_DEFCONFIG=$PWD/configs/guest_defconfig run
```

Expected output:

```
guest: starting
guest: loaded, serving
guest alive via vpl011
```

## What making it reachable immediately found

A latent bug in the release profile: `spawn_supervision_thread()` was failing
with `no_memory` because `user_thread_count` (10, of which 9 are usable) was
exhausted by the service graph. `supervise()` returned 7 and `init` exited,
so the production profile had been booting **without** its supervision thread
— and therefore without restart-on-fault — while the console log looked
entirely healthy.

It was invisible because release is the only profile that runs `supervise()`,
there were no diagnostics past the console check, and the failure happens
after every service has already reported ready. `supervise()` now reports
which step failed, and the pool is 12 (the maximum the per-CPU pinning
`static_assert` allows at 4 CPUs).
