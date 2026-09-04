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
    "shell FAILED"
    "sh: command not found"
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

# Drives a real booted shell through a scripted session and asserts on the
# transcript, rather than on a service reporting its own readiness -- this
# is the one thing no boot-marker check above can prove: that a command
# typed at the console actually forks, execs, and produces the right
# output through a real pipeline and a real redirection.
#
# BLOCK_IMAGE=- deliberately: no disk is attached, so the virtio driver
# never runs its bring-up probe at all. This profile only exercises fork,
# exec, and ramfs (/tmp), which need no disk -- and virtio's own
# completion wait is a fixed-iteration poll with no blocking primitive
# (see src/user/drivers/virtio/main.cc), so it can and does time out under
# host contention. Coupling this profile to that would make it the
# flakiest thing in the suite for a boot path it does not exercise. Real
# ext2 reads are already proven by vfs-probe during every boot and by the
# "vfs verified" assertion above.
run_shell_profile() {
    local name="release (shell smoke test)"
    printf '\n== %s ==\n' "$name"
    local log script_fifo
    log=$(mktemp)
    script_fifo=$(mktemp -u)
    mkfifo "$script_fifo"
    trap 'rm -f "$log" "$script_fifo"' RETURN

    local variant=release
    local make_args=(ARCH="$arch" PLATFORM="$platform" BUILD_VARIANT="$variant"
                     KCONFIG_DEFCONFIG="$repo_root/configs/release_defconfig")
    local objtree="$repo_root/out/build/$arch/$platform/$variant"
    rm -f "$objtree/.config" "$objtree/include/generated/auto.conf" \
          "$objtree/include/generated/autoconf.h"

    if ! make "${make_args[@]}" all >"$log" 2>&1; then
        echo "  BUILD FAILED"
        sed -n '/error/,+3p' "$log" | head -20
        failures=$((failures + 1))
        return
    fi

    # Opened read-write, not written-then-closed: a closed write end hands
    # the guest's console reader an EOF the moment the script drains, and
    # that reader has no notion of "no more input is coming" to react to
    # -- it would just spin on a source that will never produce another
    # byte. Keeping our own read end open on fd 9 means the pipe always
    # has a writer, so the guest simply sees "nothing available yet" once
    # it has caught up, exactly like an interactive terminal that has not
    # been typed into in a while.
    exec 9<>"$script_fifo"

    # Backgrounded, not run inline: the commands below have to be typed
    # -- see the pacing comment -- while qemu is running, not before it
    # starts or after it exits.
    BLOCK_IMAGE=- timeout "$timeout_seconds" make "${make_args[@]}" run <&9 >"$log" 2>&1 &
    local qemu_job=$!

    # Poll for the prompt rather than sleeping a guessed boot time: how
    # long boot takes depends on host load, which this script does not
    # control and has seen vary by 10x within a single session.
    local waited=0
    while [ "$waited" -lt "$timeout_seconds" ] && ! grep -qF -- "shell ready" "$log"; do
        sleep 1
        waited=$((waited + 1))
    done

    # Paced, not written as one burst: the console reads one byte at a
    # time over IPC (see io.cc's read()), and a whole script's worth of
    # bytes landing before the guest is ready to drain them overruns
    # whatever buffering that path has. Observed directly: a burst write
    # here made the shell's read_line() stall silently partway through
    # echoing the SECOND command, with no error and no further byte ever
    # accepted -- not a hang in cat, a hang in reading cat's own command
    # line. A human typing does not hit this because typing is naturally
    # paced; a script faking that pacing is what avoids it.
    #
    # `cat /tmp/smoke.txt | cat` is the pipeline proof: two forked,
    # exec'd `cat` processes chained through a ramfs temp file (this
    # kernel has no pipe object -- see sh/main.cc's run_pipeline()).
    sleep 1
    printf 'echo smoke-shell-redirect-ok > /tmp/smoke.txt\r' >&9
    sleep 1
    printf 'cat /tmp/smoke.txt\r' >&9
    sleep 1
    printf 'cat /tmp/smoke.txt | cat\r' >&9
    sleep 2

    # The kernel never exits, so tearing qemu down here is the normal
    # path, not a failure -- timeout forwards this signal to the qemu
    # process it is monitoring, the same mechanism that ends every other
    # profile in this script once its own timeout elapses.
    kill "$qemu_job" 2>/dev/null
    wait "$qemu_job" 2>/dev/null
    exec 9<&-

    if ! grep -qF -- "shell ready" "$log"; then
        echo "  MISSING : shell ready"
        failures=$((failures + 1))
        return
    fi
    echo "  ok      : shell ready"

    # >= 3: once in the console's own echo of the `echo ... >` command line
    # (the marker is literally that command's argument, so this occurrence
    # proves nothing on its own), once from the direct read proving
    # redirection worked, once from the piped read proving fork+exec+
    # pipeline worked. Fewer than 3 would not distinguish "both reads
    # worked" from "the pipeline silently produced nothing."
    local marker_count
    marker_count=$(grep -cF -- "smoke-shell-redirect-ok" "$log")
    if [ "$marker_count" -ge 3 ]; then
        echo "  ok      : redirect + pipeline output (seen $marker_count times)"
    else
        echo "  MISSING : redirect + pipeline output (seen $marker_count time(s), need >= 3)"
        failures=$((failures + 1))
    fi

    local marker
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

run_shell_profile

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
