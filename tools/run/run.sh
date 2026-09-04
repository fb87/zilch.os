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
# Opt-in only: the kernel's SMMU driver (docs/architecture/
# KERNEL_ARCH_PLATFORM_DECOUPLING.md) is discovery-only and never enables
# translation, but whether enabling QEMU's virt SMMUv3 model at all
# affects other emulated devices' DMA paths (virtio-blk, virtio-net) has
# not been validated here. Default stays off so existing behavior is
# unchanged; set QEMU_SMMU=1 to add `iommu=smmuv3` for testing the driver.
smmu_opt=""
[ "${QEMU_SMMU:-}" = "1" ] && smmu_opt=",iommu=smmuv3"

case "$machine" in
    AArch64)
        dtb=$(mktemp)
        # Backing store for the virtio block device the userspace virtio
        # driver probes for. QEMU populates a virtio-mmio transport only when
        # a device is actually attached, so without this the driver's scan
        # correctly reports an empty window. BLOCK_IMAGE overrides it;
        # BLOCK_IMAGE=- omits the device entirely.
        disk=""
        blockdev=""
        if [ "${BLOCK_IMAGE:-}" != "-" ]; then
            disk=${BLOCK_IMAGE:-$(mktemp)}
            [ -s "$disk" ] || dd if=/dev/zero of="$disk" bs=1M count=16 status=none
            blockdev="1"
        fi
        trap 'rm -f "$dtb"; [ -n "${BLOCK_IMAGE:-}" ] || rm -f "$disk"' EXIT HUP INT TERM
        qemu-system-aarch64 -machine "virt,gic-version=3,virtualization=on,dumpdtb=$dtb$smmu_opt" \
            -cpu cortex-a57 -smp "$cpus" -m "${memory_mb}M" -display none
        # force-legacy=false selects the modern (VIRTIO 1.x, MMIO version 2)
        # transport. QEMU's virt board otherwise presents these as legacy
        # version 1, confirmed by the driver's own probe reading version=0x1
        # without this -- and legacy uses an entirely different queue setup
        # (QueuePFN/GuestPageSize) than the split desc/driver/device address
        # registers the modern layout uses.
        if [ -n "$blockdev" ]; then
            set -- -drive "if=none,file=$disk,format=raw,id=blk0" \
                -device virtio-blk-device,drive=blk0 "$@"
        fi
        run_arm64_qemu -machine "virt,gic-version=3,virtualization=on$smmu_opt" -cpu cortex-a57 \
            -smp "$cpus" -m "${memory_mb}M" -nographic -no-reboot -kernel "$kernel" \
            -global virtio-mmio.force-legacy=false \
            -device "loader,file=$dtb,addr=0x48000000,force-raw=on" "$@"
        ;;
    *X86-64*)
        cat >&2 << 'EOFNOTE'
Note: x86_64 kernel built successfully, but QEMU's multiboot loader only supports
32-bit kernels. To run the amd64 kernel, use one of these options:

1. Real Hardware: Boot on real x86_64 hardware with a multiboot bootloader (GRUB2)
2. UEFI: Use OVMF firmware: qemu-system-x86_64 -bios /path/to/OVMF.fd -kernel "$kernel"
3. Bootloader: Create a minimal multiboot bootloader stub
4. KVM: Use KVM instead of TCG for better compatibility

The kernel builds without errors and is ready for these environments.
EOFNOTE
        echo "error: amd64 kernel requires external bootloader or hardware support" >&2
        exit 1
        ;;
    *) echo "error: unsupported ELF machine: $machine" >&2; exit 1 ;;
esac
