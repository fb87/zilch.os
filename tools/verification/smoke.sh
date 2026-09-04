#!/usr/bin/env bash
# Boot smoke test for the profiles the certification suite cannot cover.
#
# CONFIG_SELFTEST=y replaces init's main() with the certification suite, so
# the suite structurally cannot exercise root_graph.hh's supervise() -- the
# service graph that actually ships. That gap is not theoretical: the release
# profile booted for some time with its supervision thread failing to spawn
# (and therefore no restart-on-fault) while the console log looked healthy,
# because the failure happens after every service has reported ready and
# nothing asserted on the end state.
#
# This asserts on that end state. It is deliberately output-based rather than
# exit-code-based: these profiles run forever by design, so "booted correctly"
# means "printed these markers and none of the failure markers".
set -uo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo_root"

arch=${ARCH:-arm64}
platform=${PLATFORM:-qemu-arm64-virt}
timeout_seconds=${SMOKE_TIMEOUT:-45}
failures=0

# Built once, ahead of any profile: content is architecture-independent, so
# rebuilding it per profile would just repeat identical work. Only the
# release profile below actually points BLOCK_IMAGE at it -- the guest
# profile has never depended on VFS and stays exactly as it was.
disk_image="$repo_root/out/image/disk.img"
if ! "$repo_root/tools/image/make_disk_image.sh" "$repo_root/tools/image/rootfs" "$disk_image" \
    >/dev/null 2>&1; then
    echo "error: failed to build the ext2 disk image (is e2fsprogs installed?)" >&2
    exit 1
fi

# Any of these appearing means a service reported its own failure. Listed
# explicitly rather than grepping for "FAIL" so that unrelated text
# containing that substring cannot silently make this vacuous.
# NOTE: console-server's write op is NUL-terminated and capped at
# console_write_max_bytes (24), so anything longer is silently truncated --
# which is why these are short and must match the emitted strings exactly.
# An earlier version of this list did not, and every entry was dead.
failure_markers=(
    "block-service FAILED"
    "spawn-argv FAILED"
    "fork-exec FAILED"
    "libc FAILED"
    "vfs FAILED"
    "sup: spawn failed"
    "sup: no thread"
    "sup: bind failed"
    "sup: no bin/init"
    "guest: res failed"
    "guest: mint failed"
    "guest: launch failed"
    "guest: load failed"
    "virtio: sector round trip FAIL"
    "restart FAILED"
)

run_profile() {
    local name=$1 defconfig=$2 variant=$3
    shift 3
    local expected=("$@")

    printf '\n== %s ==\n' "$name"
    local log
    log=$(mktemp)
    trap 'rm -f "$log"' RETURN

    # KCONFIG_DEFCONFIG must be passed explicitly for EVERY profile, never
    # left to mk/config.mk's default. That default uses ?=, and config.mk
    # exports the variable -- so when this script runs under `make smoke`,
    # the inner make inherits the OUTER invocation's defconfig from the
    # environment and it silently wins. That built the release tree with the
    # debug config (CONFIG_SELFTEST=1), which runs the certification suite
    # instead of the service graph this is meant to check.
    local make_args=(ARCH="$arch" PLATFORM="$platform" BUILD_VARIANT="$variant"
                     KCONFIG_DEFCONFIG="$repo_root/$defconfig")

    # Force the Kconfig regeneration. mk/config.mk's rule depends on the
    # defconfig as a FILE, not on which defconfig was selected, so pointing
    # it at a different one does not invalidate an already-newer generated
    # config -- the tree silently keeps whatever config it was last built
    # with. Since this script deliberately builds several profiles into
    # different trees, it has to drop the generated config each time.
    local objtree="$repo_root/out/build/$arch/$platform/$variant"
    rm -f "$objtree/.config" "$objtree/include/generated/auto.conf" \
          "$objtree/include/generated/autoconf.h"

    if ! make "${make_args[@]}" all >"$log" 2>&1; then
        echo "  BUILD FAILED"
        sed -n '/error/,+3p' "$log" | head -20
        failures=$((failures + 1))
        return
    fi

    # The kernel never exits, so the timeout is the normal path; its exit
    # status is not a verdict and is deliberately ignored.
    timeout "$timeout_seconds" make "${make_args[@]}" run >"$log" 2>&1 || true

    local marker
    for marker in "${expected[@]}"; do
        if grep -qF -- "$marker" "$log"; then
            echo "  ok      : $marker"
        else
            echo "  MISSING : $marker"
            failures=$((failures + 1))
        fi
    done
    for marker in "${failure_markers[@]}"; do
        if grep -qF -- "$marker" "$log"; then
            echo "  FAILURE : $marker"
            failures=$((failures + 1))
        fi
    done
}

# Production graph: every service up, and critically the supervision thread
# started -- "graph ready" is printed only after that succeeds.
BLOCK_IMAGE="$disk_image" run_profile "release (service graph)" "configs/release_defconfig" release \
    "console-server alive" \
    "block-service verified" \
    "spawn-argv verified" \
    "fork-exec verified" \
    "libc verified" \
    "vfs verified" \
    "graph ready"

# Guest hosting: proves stage-2 trap-and-emulate through the domain manager's
# vPL011 reaches the real UART.
# Also the only profile with CONFIG_FAULT_INJECTION, so it is where
# restart-on-fault is proven: root crashes the device role and confirms the
# supervision thread restarted it into a role that answers health checks.
run_profile "guest (vPL011 hosting + restart)" "configs/guest_defconfig" development \
    "graph ready" \
    "restart ok" \
    "guest: loaded, serving" \
    "guest alive via vpl011"

printf '\n'
if [ "$failures" -eq 0 ]; then
    echo "smoke: PASS"
    exit 0
fi
echo "smoke: FAIL ($failures problem(s))"
exit 1
