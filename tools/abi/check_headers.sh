#!/bin/sh
set -eu
root=${1:?source root required}
cxx=${CXX:-clang++}
tmp=${TMPDIR:-/tmp}/zilch-abi-headers-$$
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT INT TERM

find "$root/include/abi" -type f -name '*.hh' | sort | while IFS= read -r header; do
    rel=${header#"$root/include/abi/"}
    unit="$tmp/$(printf '%s' "$rel" | tr '/' '_').cc"
    printf '#include <%s>\nint main() { return 0; }\n' "$rel" > "$unit"
    "$cxx" -std=c++20 -Wall -Wextra -Werror -pedantic-errors \
        -I"$root/include/abi" -I"$root/include" -fsyntax-only "$unit"
done

echo 'ABI header self-containment: PASS'
