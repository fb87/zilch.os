#!/usr/bin/env sh
set -eu
self_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
kernel=${1:-}
if [ -z "$kernel" ]; then
    case "$(uname -m)" in
        aarch64|arm64) kernel="$self_dir/../build/arm64/qemu-arm64-virt/zilch.elf" ;;
        x86_64|amd64) kernel="$self_dir/../build/amd64/qemu-amd64-q35/zilch.elf" ;;
        *) echo "error: specify kernel ELF" >&2; exit 1 ;;
    esac
else
    shift
fi
[ -f "$kernel" ] || { echo "error: kernel not found: $kernel" >&2; exit 1; }
readelf_tool=$(command -v llvm-readelf 2>/dev/null || command -v readelf)
machine=$($readelf_tool -h "$kernel" | awk -F: '/Machine:/ {gsub(/^[[:space:]]+/, "", $2); print $2; exit}')
case "$machine" in
    AArch64) exec qemu-system-aarch64 -machine virt,gic-version=3,virtualization=on -cpu cortex-a57 -smp 2 -m 256M -nographic -no-reboot -kernel "$kernel" "$@" ;;
    *X86-64*) exec qemu-system-x86_64 -machine q35 -cpu max -smp 2 -m 256M -nographic -no-reboot -kernel "$kernel" "$@" ;;
    *) echo "error: unsupported ELF machine: $machine" >&2; exit 1 ;;
esac
