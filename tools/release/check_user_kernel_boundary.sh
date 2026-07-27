#!/bin/sh
set -eu
root=${1:?repository root required}
for stale in \
    "$root/src/arch/arm64/boot/user.S" \
    "$root/src/arch/arm64/boot/guest_blob.S" \
    "$root/src/kernel/include/sys/kernel/acceptance" \
    "$root/src/kernel/include/sys/kernel/scheduler_test.hh"; do
    [ ! -e "$stale" ] || { echo "error: user/test implementation remains in production tree: $stale" >&2; exit 1; }
done
[ -f "$root/src/user/bootstrap/embedded_images.S" ] || { echo 'error: missing user-owned bootstrap image bundle' >&2; exit 1; }
[ -f "$root/tests/hypervisor/fixtures/guest_blob.S" ] || { echo 'error: missing test-owned guest blob adapter' >&2; exit 1; }
if grep -R -n -E 'namespace sys::kernel::acceptance|scheduler_test' "$root/src/kernel" --include='*.hh' --include='*.cc' --include='*.S'; then
    echo 'error: certification implementation leaked into production kernel tree' >&2
    exit 1
fi
echo 'user/kernel ownership boundary: PASS'
