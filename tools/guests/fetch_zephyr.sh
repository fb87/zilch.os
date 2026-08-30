#!/bin/sh
set -eu

[ "$#" -le 1 ] || { echo "usage: $0 [destination]" >&2; exit 2; }

destination=${1:-third_party/zephyr}
repository=https://github.com/zephyrproject-rtos/zephyr.git
revision=8469084dfae85f854555f0607f2c838dad097235

if [ -e "$destination" ]; then
    echo "error: destination already exists: $destination" >&2
    exit 1
fi

git clone --depth 1 "$repository" "$destination"
git -C "$destination" fetch --depth 1 origin "$revision"
git -C "$destination" checkout --detach "$revision"
