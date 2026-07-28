#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

root=${1:?source root required}
cxx=${CXX:-clang++}
out=${TMPDIR:-/tmp}/zilch-abi-ubsan
trap 'rm -f "$out"' EXIT HUP INT TERM
"$cxx" -std=c++20 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined \
  -I"$root/include" -I"$root/include/abi" "$root/tests/abi/layout.cc" -o "$out"
UBSAN_OPTIONS=halt_on_error=1 "$out"
echo "ABI UBSan check: PASS"
