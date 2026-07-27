#!/bin/sh
set -eu
root=${1:?source root required}
cxx=${CXX:-clang++}
"$cxx" -std=c++20 -Wall -Wextra -Werror -I"$root/include" -I"$root/include/abi" \
  "$root/tests/abi/layout.cc" -o "${TMPDIR:-/tmp}/zilch-abi-layout"
"${TMPDIR:-/tmp}/zilch-abi-layout"
echo "ABI layout check: PASS"
