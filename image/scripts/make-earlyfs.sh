#!/bin/sh
set -eu
[ "$#" -eq 3 ] || { echo "usage: $0 <init.elf> <manifest> <output.tar>" >&2; exit 2; }
init=$1
manifest=$2
output=$3
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p "$stage/bin" "$stage/etc" "$stage/lib"
install -m 0755 "$init" "$stage/bin/init"
install -m 0644 "$manifest" "$stage/etc/system.toml"
tar -C "$stage" -cf "$output" .
