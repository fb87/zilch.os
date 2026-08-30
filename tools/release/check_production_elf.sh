#!/bin/sh
set -eu
elf=${1:?kernel ELF required}
command -v strings >/dev/null 2>&1 || { echo 'error: strings not found' >&2; exit 1; }
nm_tool=$(command -v llvm-nm 2>/dev/null || command -v nm 2>/dev/null) || {
    echo 'error: nm not found' >&2
    exit 1
}
readelf_tool=$(command -v llvm-readelf 2>/dev/null || command -v readelf 2>/dev/null) || {
    echo 'error: readelf not found' >&2
    exit 1
}
if "$readelf_tool" -S "$elf" | grep -E '[[:space:]]\.debug_|[[:space:]]\.zdebug_' >/dev/null; then
    echo 'error: production ELF contains DWARF debug sections' >&2
    exit 1
fi
for token in \
    '[ACCEPTANCE]' \
    '[GUEST]' \
    'acceptance_worker_tick' \
    'hypervisor_self_test' \
    'hypervisor_control_model' \
    'root fuzz cpu='; do
    if strings "$elf" | grep -F "$token" >/dev/null; then
        echo "error: production ELF contains test-only marker: $token" >&2
        exit 1
    fi
done
for symbol in sys_arm64_guest_image_start sys_arm64_guest_image_end; do
    if "$nm_tool" "$elf" | grep -F "$symbol" >/dev/null; then
        echo "error: production ELF contains test guest symbol: $symbol" >&2
        exit 1
    fi
done
