#!/bin/sh
set -eu
root=${1:?repository root required}

[ -f "$root/src/kernel/kernel.mk" ] || { echo 'error: missing src/kernel/kernel.mk' >&2; exit 1; }
[ -f "$root/src/user/user.mk" ] || { echo 'error: missing src/user/user.mk' >&2; exit 1; }
[ -f "$root/src/image/image.mk" ] || { echo 'error: missing src/image/image.mk' >&2; exit 1; }
[ -f "$root/tests/tests.mk" ] || { echo 'error: missing tests/tests.mk' >&2; exit 1; }
[ ! -f "$root/tools/build/Makefile.user" ] || { echo 'error: obsolete monolithic userspace make fragment remains' >&2; exit 1; }

if grep -R -n -E '#[[:space:]]*include[[:space:]]*[<"]sys/kernel/' "$root/src/user" --include='*.cc' --include='*.hh' --include='*.S'; then
    echo 'error: userspace includes private kernel headers' >&2
    exit 1
fi
if grep -R -n -E '#[[:space:]]*include[[:space:]]*[<"]sys/(user|certification)' "$root/src/kernel" --include='*.cc' --include='*.hh' --include='*.S'; then
    echo 'error: kernel includes private userspace or certification headers' >&2
    exit 1
fi
if grep -R -n -E '#[[:space:]]*include.*(src/kernel|src/user|tests/)' "$root/include/abi" --include='*.hh' --include='*.h'; then
    echo 'error: public ABI includes a private implementation header' >&2
    exit 1
fi
if grep -n -E '(^|[^a-zA-Z0-9_/])tests/include([^a-zA-Z0-9_/]|$)' "$root/src/user/user.mk"; then
    echo 'error: userspace build can see kernel verification include tree' >&2
    exit 1
fi
if grep -n 'src/kernel/include' "$root/src/user/user.mk"; then
    echo 'error: userspace build can see kernel private include tree' >&2
    exit 1
fi
echo 'build ownership boundary: PASS'
