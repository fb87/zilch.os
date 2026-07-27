#!/bin/sh
set -eu
root=${1:?repository root required}
old="$root/src/arch/arm64/boot/guest.S"
[ ! -e "$old" ] || { echo "error: guest payload remains under architecture boot tree: $old" >&2; exit 1; }
guest_root="$root/src/user/guests"
[ -d "$guest_root" ] || { echo "error: missing src/user/guests" >&2; exit 1; }
if grep -R -n -E '#include[[:space:]]+[<"]sys/kernel/' "$guest_root" --include='*.cc' --include='*.hh' --include='*.S'; then
    echo 'error: guest source includes private kernel headers' >&2
    exit 1
fi
if find "$guest_root" -type f \( -name '*.cc' -o -name '*.hh' -o -name '*.S' \) -print0 | xargs -0 grep -n -E 'sys::kernel|kernel::' 2>/dev/null; then
    echo 'error: guest source references private kernel namespaces' >&2
    exit 1
fi
echo 'guest ownership boundary: PASS'
