#!/usr/bin/env sh
set -eu
self_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
kernel=${1:-}
if [ -z "$kernel" ]; then
    case "$(uname -m)" in
        aarch64|arm64) kernel="$self_dir/../../out/build/arm64/qemu-arm64-virt/zilch.elf" ;;
        x86_64|amd64) kernel="$self_dir/../../out/build/amd64/qemu-amd64-q35/zilch.elf" ;;
        *) echo "error: specify kernel ELF" >&2; exit 1 ;;
    esac
else
    shift
fi
[ -f "$kernel" ] || { echo "error: kernel not found: $kernel" >&2; exit 1; }
readelf_tool=$(command -v llvm-readelf 2>/dev/null || command -v readelf)
machine=$($readelf_tool -h "$kernel" | awk -F: '/Machine:/ {gsub(/^[[:space:]]+/, "", $2); print $2; exit}')
cpus=${CPUS:-4}
memory_mb=${MEMORY_MB:-256}

# The certification suite measures guest-observed tick deltas against fixed
# limits (scheduler_latency_bounds, ipc_latency_bound). Under TCG, a vCPU
# that loses its host CPU to unrelated load still has the guest's virtual
# timer advancing, so host contention shows up as enormous apparent
# in-guest latency and fails those bounds -- a measurement artifact of the
# environment, not kernel behavior. Observed directly: identical builds
# reported ipc_latency max_ticks anywhere from 135569 (dedicated cores) to
# 1406527 (contended) against a 620000 limit, and a stashed-baseline
# comparison confirmed the same failures with the tree's own changes
# reverted.
#
# So pin QEMU to the top half of the host's CPUs by default, reserving the
# rest for everything else, instead of leaving it to whoever remembers to
# set QEMU_CPUSET (samples/guests/zephyr already hardcoded 4-7 for exactly
# this reason). Explicit QEMU_CPUSET still wins; QEMU_CPUSET=- opts out
# entirely. Skipped when the host has too few cores to spare any, or when
# taskset is unavailable.
if [ -z "${QEMU_CPUSET:-}" ] && command -v taskset >/dev/null 2>&1; then
    host_cpus=$(command -v nproc >/dev/null 2>&1 && nproc || echo 0)
    if [ "$host_cpus" -ge $((cpus * 2)) ]; then
        QEMU_CPUSET="$((host_cpus - cpus))-$((host_cpus - 1))"
    fi
fi
run_arm64_qemu() {
    if [ -n "${QEMU_CPUSET:-}" ] && [ "${QEMU_CPUSET}" != "-" ]; then
        exec taskset -c "$QEMU_CPUSET" qemu-system-aarch64 "$@"
    fi
    exec qemu-system-aarch64 "$@"
}
case "$machine" in
    AArch64)
        dtb=$(mktemp)
        trap 'rm -f "$dtb"' EXIT HUP INT TERM
        qemu-system-aarch64 -machine "virt,gic-version=3,virtualization=on,dumpdtb=$dtb" \
            -cpu cortex-a57 -smp "$cpus" -m "${memory_mb}M" -display none
        run_arm64_qemu -machine virt,gic-version=3,virtualization=on -cpu cortex-a57 \
            -smp "$cpus" -m "${memory_mb}M" -nographic -no-reboot -kernel "$kernel" \
            -device "loader,file=$dtb,addr=0x48000000,force-raw=on" "$@"
        ;;
    *X86-64*) exec qemu-system-x86_64 -machine q35 -cpu max -smp "$cpus" -m "${memory_mb}M" -nographic -no-reboot -kernel "$kernel" "$@" ;;
    *) echo "error: unsupported ELF machine: $machine" >&2; exit 1 ;;
esac
