#!/bin/sh
set -eu
[ "$#" -eq 5 ] || { echo "usage: $0 <init.elf> <memory-server.elf> <pager-client.elf> <manifest> <output.tar>" >&2; exit 2; }
init=$1
memory_server=$2
pager_client=$3
manifest=$4
output=$5
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p "$stage/bin" "$stage/etc" "$stage/lib"
install -m 0755 "$init" "$stage/bin/init"
install -m 0755 "$memory_server" "$stage/bin/memory-server"
install -m 0755 "$pager_client" "$stage/bin/pager-client"
install -m 0644 "$manifest" "$stage/etc/system.toml"
tar -C "$stage" -cf "$output" .
