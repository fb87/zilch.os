#!/bin/sh
set -eu
[ "$#" -eq 7 ] || { echo "usage: $0 <init.elf> <memory-server.elf> <pager-client.elf> <memory-client.elf> <manifest> <output.tar> <guest-test.elf|->" >&2; exit 2; }
init=$1
memory_server=$2
pager_client=$3
memory_client=$4
manifest=$5
output=$6
guest_test=$7
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p "$stage/bin" "$stage/etc" "$stage/lib" "$stage/guests"
install -m 0755 "$init" "$stage/bin/init"
install -m 0755 "$memory_server" "$stage/bin/memory-server"
install -m 0755 "$pager_client" "$stage/bin/pager-client"
install -m 0755 "$memory_client" "$stage/bin/memory-client"
if [ "$guest_test" != "-" ]; then
    install -m 0755 "$guest_test" "$stage/guests/test-arm64.elf"
fi
install -m 0644 "$manifest" "$stage/etc/system.toml"
tar \
    --sort=name \
    --mtime="@${SOURCE_DATE_EPOCH:-0}" \
    --owner=0 --group=0 --numeric-owner \
    -C "$stage" -cf "$output" .
