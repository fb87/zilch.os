#!/usr/bin/env bash
#
# Repeated cold boot of the release service graph.
#
# What this guards, specifically: the boot stall. Two concurrent creators
# could claim the same user thread slot, and the loser silently never ran --
# so a boot either came up normally or hung with no output and no fault,
# depending on timing. A single boot proves nothing against that class of
# defect, which is why it survived every gate in this tree until it was
# hunted deliberately. Repetition is the only thing that finds it.
#
# It is therefore a RATE test, not a pass/fail of one run: report how many
# boots of N reached a fully-ready service graph, and fail if any did not.
#
# Deliberately cold boots rather than warm reboots. The kernel runs with
# -no-reboot and has no reset path back through firmware, so "reboot" here
# means a fresh machine each time, which is also the case that exercises
# discovery, SMP bringup and the service graph from nothing.
#
# Usage: BOOT_REPEAT_COUNT=20 tools/verification/boot_repeat.sh
set -u

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo_root"

arch=${ARCH:-arm64}
platform=${PLATFORM:-qemu-arm64-virt}
variant=release
# 12 is a compromise: enough repetitions to make a timing-dependent stall
# likely to show at least once, few enough to run inside a normal review
# cycle at roughly 40s a boot. Raise it when chasing something specific.
count=${BOOT_REPEAT_COUNT:-12}
boot_timeout=${BOOT_REPEAT_TIMEOUT:-45}

make_args=(ARCH="$arch" PLATFORM="$platform" BUILD_VARIANT="$variant"
           KCONFIG_DEFCONFIG="$repo_root/configs/release_defconfig")

# Same reason smoke.sh does this: the generated config is not invalidated by
# pointing KCONFIG_DEFCONFIG somewhere else, so a tree previously built with
# another profile would silently keep it.
objtree="$repo_root/out/build/$arch/$platform/$variant"
rm -f "$objtree/.config" "$objtree/include/generated/auto.conf" \
      "$objtree/include/generated/autoconf.h"

build_log=$(mktemp)
trap 'rm -f "$build_log"' EXIT

printf '\n== repeated cold boot (%d iterations) ==\n' "$count"

if ! make "${make_args[@]}" all >"$build_log" 2>&1; then
    echo "  BUILD FAILED"
    sed -n '/error/,+3p' "$build_log" | head -20
    exit 1
fi

disk_image="$repo_root/out/image/disk.img"
if [ ! -f "$disk_image" ]; then
    "$repo_root/tools/image/make_disk_image.sh" "$repo_root/tools/image/rootfs" \
        "$disk_image" >/dev/null 2>&1 || true
fi

# Every marker the service graph publishes when it is genuinely up. Checking
# only "graph ready" would accept a boot that reached the end with a role
# missing, which is the other half of what repetition is meant to catch.
markers=(
    "console-server alive"
    "block-service verified"
    "spawn-argv verified"
    "fork-exec verified"
    "libc verified"
    "vfs verified"
    "graph ready"
)

ready=0
failed_runs=()
for iteration in $(seq 1 "$count"); do
    log=$(mktemp)
    # The kernel never exits, so the timeout is the normal path and its exit
    # status is not a verdict.
    BLOCK_IMAGE="$disk_image" timeout "$boot_timeout" \
        make "${make_args[@]}" run >"$log" 2>&1 || true

    missing=""
    for marker in "${markers[@]}"; do
        if ! grep -qF -- "$marker" "$log"; then
            missing="$missing${missing:+, }$marker"
        fi
    done

    # A kernel error line means the boot got somewhere bad even if the
    # markers happened to appear.
    errors=$(grep -c "\[ERR\]" "$log" || true)

    if [ -z "$missing" ] && [ "$errors" -eq 0 ]; then
        ready=$((ready + 1))
        printf '  boot %2d/%d: ok\n' "$iteration" "$count"
    else
        printf '  boot %2d/%d: FAILED (%s%s)\n' "$iteration" "$count" \
            "${missing:-no missing markers}" \
            "$([ "$errors" -gt 0 ] && printf '; %d [ERR] line(s)' "$errors")"
        failed_runs+=("$iteration")
        # Keep the evidence for a failing run; passing runs are not worth the
        # disk, and a stall reproduces rarely enough that losing its log is
        # the expensive mistake.
        cp "$log" "$repo_root/out/boot_repeat_failure_${iteration}.log"
        printf '            log: out/boot_repeat_failure_%d.log\n' "$iteration"
    fi
    rm -f "$log"
done

printf '\n  reached a ready service graph: %d/%d\n' "$ready" "$count"
if [ "$ready" -ne "$count" ]; then
    printf '  FAIL: boots %s did not come up\n' "${failed_runs[*]}"
    exit 1
fi
printf '  boot-repeat: PASS\n'
