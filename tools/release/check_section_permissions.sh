#!/bin/sh
set -eu
elf=${1:?ELF image required}
readelf_tool=${READELF:-$(command -v llvm-readelf 2>/dev/null || command -v readelf)}

bad=$($readelf_tool -W -S "$elf" | awk '
/^  \[[[:space:]]*[0-9]+\]/ {
    name=$3; flags=$9;
    if (flags ~ /W/ && flags ~ /X/) {
        print "W+X section: " name;
        failed=1;
    }
    if (name == ".text" && flags !~ /X/) {
        print "non-executable text section: " name;
        failed=1;
    }
    if (name == ".rodata" && flags ~ /W/) {
        print "writable rodata section: " name;
        failed=1;
    }
}
END { if (failed) exit 1 }
') || {
    echo "section permission audit failed for $elf" >&2
    exit 1
}
if [ -n "$bad" ]; then
    printf '%s\n' "$bad" >&2
    exit 1
fi
echo "  SECTION-PERMISSIONS $elf: PASS"
