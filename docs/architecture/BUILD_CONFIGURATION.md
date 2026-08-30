# Build configuration architecture

Status: **planned migration**. The current build still derives configuration
directly from `BUILD_VARIANT` in `mk/config.mk`. This document defines the
target configuration contract and must not be cited as completed evidence until
the generated Kconfig artifacts and gates described below exist.

## Configuration source of truth

Zilch will use a top-level Kconfig hierarchy as the sole source of product,
debug, test, diagnostic, and guest feature selection. Kconfig processing will
generate these files below the selected object directory:

```text
.config
include/generated/autoconf.h
include/generated/auto.conf
```

C, C++, and assembly sources consume `autoconf.h`. Make dependency selection
consumes `auto.conf`. Make variables may select an input defconfig or provide an
external artifact path, but they must not independently redefine a generated
`CONFIG_*` value.

The supported configuration entry points will be:

```text
make menuconfig
make olddefconfig
make debug
make release
```

Kconfiglib is the selected parser and menuconfig implementation. It is a core
configuration dependency, not a guest build dependency.

## Build profiles

Zilch supports two profile presets.

### Debug

The debug defconfig enables tests, assertions, debug information, retained
diagnostics, and tracing:

```text
CONFIG_DEBUG=y
CONFIG_TESTS=y
CONFIG_SELFTEST=y
CONFIG_HYPERVISOR_SELFTEST=y
CONFIG_VERBOSE_DIAGNOSTICS=y
CONFIG_TRACE=y
CONFIG_DEBUG_INFO=y
CONFIG_PRINTK_TIME=y
```

The intended compiler policy is `-Og -g3 -fno-omit-frame-pointer`. Debug is the
only profile allowed to compile test dispatch, certification fixtures, fuzz
decoders, acceptance reporting, and embedded verification guests.

### Release

The release defconfig enables product code and timestamps while excluding all
test and debug mechanisms:

```text
CONFIG_RELEASE=y
CONFIG_DEBUG=n
CONFIG_TESTS=n
CONFIG_SELFTEST=n
CONFIG_HYPERVISOR_SELFTEST=n
CONFIG_VERBOSE_DIAGNOSTICS=n
CONFIG_TRACE=n
CONFIG_DEBUG_INFO=n
CONFIG_PRINTK_TIME=y
```

The intended compiler policy is `-O2 -DNDEBUG`. Kconfig dependencies must make
the forbidden release combination unrepresentable, and release ELF checks must
independently reject test markers, debug strings, fixtures, and DWARF sections.

The existing `development`, `certification`, and `release` variants remain
transitional until every internal command, CI job, and evidence reference has
been migrated. Historical documents retain the names used when they were
written.

## Guest configuration

The root guest menu will expose generic policy and sample selectors:

```text
CONFIG_GUEST_SUPPORT
CONFIG_GUEST_TEST_ARM64
CONFIG_GUEST_EXTERNAL
CONFIG_GUEST_INTERACTIVE
CONFIG_GUEST_ZEPHYR
```

The built-in ARM64 verification guest depends on debug tests. External guest
images are build inputs supplied through the generic domain-manager artifact
boundary; their source trees and toolchains are not part of the core build.

Guest samples live below:

```text
samples/guests/zephyr/
samples/guests/linux/
samples/guests/freebsd/
```

Each sample owns its fetch policy, pinned source revision, nested development
shell, build orchestration, generated output, and acceptance procedure. The
root Zilch flake contains only core kernel build dependencies. Generated sample
content stays below `samples/guests/<name>/out/` and is ignored by Git.

The Zephyr migration target is `samples/guests/zephyr/`. Until the move is
implemented, its sources remain under `src/user/guests/zephyr/` and its fetch
helper remains under `tools/guests/`.

## Kernel log timestamps

When `CONFIG_PRINTK_TIME=y`, serialized kernel records use Linux-style,
boot-relative monotonic timestamps:

```text
[    0.000000] [INFO] zilch L4 microkernel 0.8.0
[    0.012345] [WARN] user fault delivered ...
```

The boot CPU captures the architectural counter baseline before the first
formatted kernel record. `printk` converts the counter delta with the calibrated
architectural frequency and emits seconds plus fixed six-digit microseconds
inside the printk serialization boundary. SMP records therefore keep the
timestamp, level, and message atomic.

Raw guest UART output, EL2 emergency output, and lock-free emergency-ring
records are not rewritten as formatted kernel records. An architecture without
a calibrated counter emits a deterministic zero timestamp until calibration is
implemented.

## Verification contract

Configuration migration is complete only when all of the following pass:

1. A clean debug build includes and runs the bounded test suite.
2. A clean release build succeeds without fetching or building any guest
   sample.
3. Release source and ELF gates find no tests, debug diagnostics, trace-only
   strings, fixtures, or DWARF sections.
4. ARM64 and AMD64 release builds remain reproducible.
5. Kernel records match the Linux timestamp prefix format.
6. The standalone Zephyr sample builds through its nested flake and its native
   shell accepts `help` through the generic external guest interface.
