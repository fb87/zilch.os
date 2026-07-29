#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

root=${1:?source root required}
cxx=${CXX:-clang++}
work=${TMPDIR:-/tmp}/zilch-host-kernel-logic
binary=$work/test
profiles=$work/profiles
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$profiles"

"$cxx" -std=c++20 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -fprofile-instr-generate -fcoverage-mapping \
  -I"$root/include" -I"$root/include/abi" \
  -I"$root/src/user/include" \
  -I"$root/src/kernel/include" -I"$root/src/arch/amd64/include" \
  "$root/tests/host/kernel_logic.cc" -o "$binary"

LLVM_PROFILE_FILE="$profiles/logic.profraw" \
  ASAN_OPTIONS=detect_leaks=0 \
  UBSAN_OPTIONS=halt_on_error=1 \
  "$binary"
llvm-profdata merge -sparse "$profiles/logic.profraw" -o "$profiles/logic.profdata"
mkdir -p "$root/out/reports"
llvm-cov report "$binary" \
  -instr-profile="$profiles/logic.profdata" \
  "$root/tests/host/kernel_logic.cc" | tee "$root/out/reports/host-kernel-coverage.txt"
if command -v clang-tidy >/dev/null 2>&1; then
  clang-tidy --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' \
    "$root/tests/host/kernel_logic.cc" -- \
    -std=c++20 -I"$root/include" -I"$root/include/abi" \
    -I"$root/src/user/include" \
    -I"$root/src/kernel/include" -I"$root/src/arch/amd64/include"
fi
echo "Host kernel logic ASan/UBSan/property/coverage: PASS"
