#!/bin/sh
set -eu
elf=${1:?kernel ELF required}
command -v strings >/dev/null 2>&1 || { echo 'error: strings not found' >&2; exit 1; }
command -v nm >/dev/null 2>&1 || { echo 'error: nm not found' >&2; exit 1; }
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
    if nm "$elf" | grep -F "$symbol" >/dev/null; then
        echo "error: production ELF contains test guest symbol: $symbol" >&2
        exit 1
    fi
done
