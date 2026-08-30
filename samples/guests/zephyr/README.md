# Zephyr ARM64 guest

This sample owns its source pin, toolchain shell, build, generated output, and
interactive acceptance. It is not a dependency of the root Zilch build. See
`docs/architecture/BUILD_CONFIGURATION.md`.

This guest targets Zephyr v4.0.0 commit
`8469084dfae85f854555f0607f2c838dad097235`. Fetch the source with
`fetch.sh`.

The initial target is deliberately minimal: one EL1 Cortex-A53 vCPU, 128 KiB
of RAM at `0x40000000` and a polling PL011 console at `0x09000000`. A
spurious-only custom interrupt controller keeps the first guest free of a GIC
dependency. Zephyr keeps its MMU enabled so the loaded image preserves W^X
permissions. It does not require a guest timer service.

Use the sample-local flake and Makefile:

```sh
nix develop
make fetch
make guest
make MODE=debug run
```

`make MODE=release run` builds the release profile with the same explicit guest
artifact. `make MODE=debug acceptance` sends a delayed `help` command and
requires the native shell command list in the output.

Zephyr bring-up acceptance requires the native `zilch:~$` prompt and a
successful `help` command response containing `Available commands:`. Build the
Zilch image with `DOMAIN_GUEST_INTERACTIVE=1` and run QEMU with `QEMU_CPUSET`
set to reserved host CPUs for stable timing.

After relocation, `samples/guests/zephyr/Makefile` will own `fetch`, `guest`,
`image`, `run`, `acceptance`, and `clean` targets. All Zephyr source checkouts,
west modules, Python dependencies, and generated artifacts will stay below the
sample-local ignored output directory.
