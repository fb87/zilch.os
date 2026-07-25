#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

[ "$#" -eq 4 ] || {
    echo "usage: $0 <source-tree> <build-root> <project> <version>" >&2
    exit 2
}

src=$(CDPATH= cd -- "$1" && pwd)
build=$2
project=$3
version=$4
name="${project}-${version}"
release_root="$src/release"
stage="$release_root/stage/$name"
dist="$release_root/dist"
archive="$dist/$name.tar.xz"

need()
{
    [ -f "$1" ] || { echo "error: missing release input: $1" >&2; exit 1; }
}

for target in arm64/qemu-arm64-virt amd64/qemu-amd64-q35; do
    need "$build/$target/$project.elf"
    need "$build/$target/$project.bin"
    need "$build/$target/$project.map"
    need "$build/$target/user/init.elf"
    need "$build/$target/image/earlyfs.tar"
done

rm -rf "$release_root/stage"
mkdir -p "$stage/source" "$stage/systems" "$dist"

tar \
    --exclude='./build' \
    --exclude='./release' \
    --exclude='./.git' \
    --exclude='./.cache' \
    -C "$src" -cf - . | tar -C "$stage/source" -xf -

copy_target()
{
    build_name=$1
    release_name=$2
    out="$stage/systems/$release_name"
    mkdir -p "$out"
    cp "$build/$build_name/$project.elf" "$out/kernel.elf"
    cp "$build/$build_name/$project.bin" "$out/kernel.bin"
    cp "$build/$build_name/$project.map" "$out/kernel.map"
    cp "$build/$build_name/user/init.elf" "$out/init.elf"
    cp "$build/$build_name/user/init.map" "$out/init.map"
    cp "$build/$build_name/image/earlyfs.tar" "$out/earlyfs.tar"
    cp "$src/image/manifests/minimal.toml" "$out/manifest.toml"
}

copy_target arm64/qemu-arm64-virt arm64-qemu-virt
copy_target amd64/qemu-amd64-q35 amd64-qemu-q35
cp "$src/scripts/run.sh" "$stage/run.sh"
chmod 0755 "$stage/run.sh"

cat > "$stage/RELEASE.md" <<EOF_RELEASE
# $project $version

This release contains a clean source export and two system-development bundles:

- ARM64 / QEMU virt
- AMD64 / QEMU q35

Each bundle includes the kernel ELF/raw image/map, the initial userspace
\`init.elf\`, and an earlyfs archive. The kernel skeleton currently boots to its
diagnostic marker; loading and entering the root task is the next boot-flow
milestone.

Run a kernel explicitly:

\`./run.sh systems/arm64-qemu-virt/kernel.elf\`
\`./run.sh systems/amd64-qemu-q35/kernel.elf\`
EOF_RELEASE

(
    cd "$stage"
    find systems -type f -print | LC_ALL=C sort | xargs sha256sum > SHA256SUMS
)

rm -f "$archive" "$archive.sha256"
tar -C "$release_root/stage" -cJf "$archive" "$name"
(
    cd "$dist"
    sha256sum "$(basename "$archive")" > "$(basename "$archive").sha256"
)
printf '  RELEASE %s\n' "$archive"
