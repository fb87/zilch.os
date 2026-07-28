#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

[ "$#" -eq 3 ] || {
    echo "usage: $0 <first-build> <second-build> <label>" >&2
    exit 2
}

first=$1
second=$2
label=$3
failed=0
for relative in \
    zilch.elf zilch.bin zilch.map \
    user/init.elf user/init.map \
    image/earlyfs.tar; do
    left="$first/$relative"
    right="$second/$relative"
    if [ ! -f "$left" ] || [ ! -f "$right" ]; then
        echo "error: missing reproducibility artifact for $label: $relative" >&2
        failed=1
        continue
    fi
    if [ "${relative##*.}" = map ]; then
        left_hash=$(sed "s#${first}#.#g" "$left" | sha256sum | awk '{print $1}')
        right_hash=$(sed "s#${second}#.#g" "$right" | sha256sum | awk '{print $1}')
    else
        left_hash=$(sha256sum "$left" | awk '{print $1}')
        right_hash=$(sha256sum "$right" | awk '{print $1}')
    fi
    if [ "$left_hash" != "$right_hash" ]; then
        echo "error: $label differs: $relative" >&2
        failed=1
    fi
done

if [ "$failed" -ne 0 ]; then
    exit 1
fi
echo "REPRODUCIBLE-BUILD $label: PASS"
