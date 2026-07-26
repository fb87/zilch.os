#!/bin/sh
set -eu
elf=${1:?kernel ELF required}
command -v strings >/dev/null 2>&1 || { echo 'error: strings not found' >&2; exit 1; }
for token in '[ACCEPTANCE]' 'hypervisor_profile_0_6' 'acceptance_worker_tick' 'hypervisor_self_test'; do
    if strings "$elf" | grep -F "$token" >/dev/null; then
        echo "error: production ELF contains test-only marker: $token" >&2
        exit 1
    fi
done
