#!/bin/sh
set -eu
elf=$1
nm_tool=${NM:-$(command -v llvm-nm 2>/dev/null || command -v nm)}
readelf_tool=${READELF:-$(command -v llvm-readelf 2>/dev/null || command -v readelf)}
forbidden='__cxa_throw|__gxx_personality_v0|__dynamic_cast|_Znwm|_Znam|malloc|free'
if "$nm_tool" -u "$elf" 2>/dev/null | grep -E "$forbidden" >/dev/null; then
  echo 'forbidden runtime symbol found' >&2; exit 1
fi
if "$readelf_tool" -S "$elf" | grep -E '\.(init_array|fini_array|eh_frame|gcc_except_table)' >/dev/null; then
  echo 'forbidden runtime section found' >&2; exit 1
fi
echo "  CHECK   $elf"
