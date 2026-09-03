#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Compiles and natively runs tests/host/kernel_logic.cc with ASan/UBSan on
# whatever machine invokes this script. It used to point at
# src/arch/amd64/include for the sys::arch::* headers the portable kernel
# logic under test transitively includes -- which happened to compile and
# run correctly on an x86_64 host (amd64's cpu.hh uses `cpuid`, unprivileged
# on x86) but failed to assemble at all on an aarch64 host ("=a" is not a
# valid register constraint for that target). src/arch/host/include exists
# so this script, and the CI/local host it runs on, no longer needs to be
# x86_64 to exercise this test -- see that tree's cpu.hh for the fuller
# story, including why simply using the host's OWN native arch tree instead
# would have been worse on aarch64, not better (arm64's cpu.hh reads an
# EL1-only register that SIGILLs from an ordinary process).
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
  -I"$root/src/kernel/include" -I"$root/src/arch/host/include" \
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
