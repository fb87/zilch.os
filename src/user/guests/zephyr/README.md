# Zephyr ARM64 guest

This guest targets Zephyr v4.0.0 commit
`8469084dfae85f854555f0607f2c838dad097235`. Fetch the source with
`tools/guests/fetch_zephyr.sh`.

The initial target is deliberately minimal: one EL1 Cortex-A53 vCPU, 128 KiB
of RAM at `0x40000000` and a polling PL011 console at `0x09000000`. A
spurious-only custom interrupt controller keeps the first guest free of a GIC
dependency. Zephyr keeps its MMU enabled so the loaded image preserves W^X
permissions. It does not require a guest timer service.

Build after fetching Zephyr and its required west modules:

```sh
west build -b qemu_cortex_a53 src/user/guests/zephyr -- \
  -DDTC_OVERLAY_FILE=hypervisor.overlay -DCONF_FILE=prj.conf
```

The resulting `zephyr.elf` is the input for the domain-manager ELF loader.
For a bounded integration build, pass its path as `DOMAIN_GUEST_ELF` to the
Zilch certification make invocation.

Zephyr bring-up acceptance requires the native `zilch:~$` prompt and a
successful `help` command response containing `Available commands:`. Build the
Zilch image with `DOMAIN_GUEST_INTERACTIVE=1` and run QEMU with `QEMU_CPUSET`
set to reserved host CPUs for stable timing.
