#!/usr/bin/env bash
#
# Reproducer for checklist 0156: console input stops being delivered.
#
# Kept as a tool because the defect is open and the expensive part of
# working on it is getting it to happen at all. The eight-command probes
# used earlier reproduced it perhaps one run in five; this one sends thirty
# short commands at 0.6s spacing, so a single run contains far more input
# events, and reproduces roughly half the time.
#
# Reports a graded result -- how many of thirty commands completed -- rather
# than pass/fail, because the stall point varies and the number is what
# makes an A/B between two builds meaningful.
#
# IMPORTANT, learned the hard way (0163, 0164): this bug's rate moves with
# host load, so a result here means nothing without a control arm measured
# in the same session. Run the two builds interleaved, never one after the
# other, and never against a remembered baseline.
#
# Usage: tools/verification/stall_repro.sh   (prints "completed: N/30")
set -u

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo_root"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

log="$work/run.log"
fifo="$work/in.fifo"
mkfifo "$fifo"
# Opened read-write so the pipe always has a writer: a closed write end
# hands the guest's console reader an EOF it has no way to react to.
exec 9<>"$fifo"

BLOCK_IMAGE=- timeout 140 make ARCH=arm64 PLATFORM=qemu-arm64-virt BUILD_VARIANT=release \
  KCONFIG_DEFCONFIG="$repo_root/configs/release_defconfig" run <&9 >"$log" 2>&1 &
job=$!

waited=0
while [ $waited -lt 60 ] && ! grep -qF "shell ready" "$log"; do sleep 1; waited=$((waited + 1)); done
sleep 1

completed=0
stalled_at=0
for index in $(seq 1 30); do
    printf 'echo m\r' >&9
    sleep 0.6
    now=$(grep -c '^m$' "$log")
    if [ "$now" -eq "$completed" ]; then
        # Give a slow run the benefit of the doubt before calling it stalled.
        sleep 2
        now=$(grep -c '^m$' "$log")
        if [ "$now" -eq "$completed" ]; then stalled_at=$index; break; fi
    fi
    completed=$now
done

kill "$job" 2>/dev/null
wait "$job" 2>/dev/null
exec 9>&-
echo "completed: $completed/30  stalled_at: $stalled_at"
