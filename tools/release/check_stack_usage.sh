#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

root=${1:?build output root required}
limit=${STACK_USAGE_LIMIT:-8192}
found=0
failed=0
for file in $(find "$root" -type f -name '*.su' -print); do
    found=1
    while IFS='	' read -r function bytes mode; do
        [ -n "${bytes:-}" ] || continue
        case "$bytes" in *[!0-9]*) continue;; esac
        if [ "$bytes" -gt "$limit" ]; then
            echo "error: stack usage ${bytes} exceeds ${limit}: ${function}" >&2
            failed=1
        fi
    done < "$file"
done
if [ "$found" -eq 0 ]; then
    echo "error: no stack-usage records found under $root" >&2
    exit 1
fi
[ "$failed" -eq 0 ] || exit 1
echo "STACK-USAGE root=$root limit=$limit: PASS"
