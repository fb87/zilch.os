#!/bin/sh
set -eu

[ "$#" -le 1 ] || { echo "usage: $0 [destination]" >&2; exit 2; }

destination=${1:-out/zephyr}
repository=https://github.com/zephyrproject-rtos/zephyr.git
revision=8469084dfae85f854555f0607f2c838dad097235
cmsis_repository=https://github.com/zephyrproject-rtos/cmsis.git
cmsis_revision=4b96cbb174678dcd3ca86e11e1f24bc5f8726da0

if [ -e "$destination" ]; then
    echo "error: destination already exists: $destination" >&2
    exit 1
fi

git clone --depth 1 "$repository" "$destination"
git -C "$destination" fetch --depth 1 origin "$revision"
git -C "$destination" checkout --detach "$revision"

workspace=$(dirname "$destination")
cmsis="$workspace/modules/hal/cmsis"
mkdir -p "$workspace/modules/hal"
git clone --depth 1 "$cmsis_repository" "$cmsis"
git -C "$cmsis" fetch --depth 1 origin "$cmsis_revision"
git -C "$cmsis" checkout --detach "$cmsis_revision"
