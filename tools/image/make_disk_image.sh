#!/usr/bin/env bash
# Builds the ext2 disk image the userspace VFS server mounts, from
# tools/image/rootfs/. See src/user/lib/ext2/include/sys/ext2/reader.hh for
# what this reader actually supports: revision-1 ext2, no extents, no
# 64bit, no journal recovery pending -- mke2fs's plain ext2 preset produces
# exactly that, which is why -t ext2 is passed explicitly rather than left
# to whatever mke2fs defaults to on the host running this script.
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <rootfs-directory> <output-image>" >&2
    exit 1
fi

rootfs=$1
output=$2

if [ ! -d "$rootfs" ]; then
    echo "error: rootfs directory not found: $rootfs" >&2
    exit 1
fi

if ! command -v mke2fs >/dev/null 2>&1; then
    echo "error: mke2fs not found (e2fsprogs) -- available in the flake devShell" >&2
    exit 1
fi

# Sized to the rootfs plus headroom for metadata and /tmp-scale future
# growth, not to whatever the host happens to have free. 8 MiB comfortably
# holds a handful of small coreutils binaries at 4096-byte block size
# without inflating boot time or repo-tracked image size.
image_size_mib=8

mkdir -p "$(dirname -- "$output")"
rm -f "$output"
truncate -s "${image_size_mib}M" "$output"

# -F forces mke2fs to proceed against a plain file rather than a block
# device, which it otherwise refuses without a prompt. -d populates the
# image directly from the given directory tree without root or a loop
# mount -- ownership/permissions on the source files are irrelevant here
# since this reader does not check them.
mke2fs -F -q -t ext2 -b 4096 -d "$rootfs" "$output" "${image_size_mib}M"
