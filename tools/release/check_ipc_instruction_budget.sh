#!/bin/sh
set -eu

elf=${1:?usage: check_ipc_instruction_budget.sh ELF}
nm_tool=${NM:-llvm-nm}

check_symbol()
{
    pattern=$1
    name=$2
    limit=$3
    size=$($nm_tool -S "$elf" | awk -v pattern="$pattern" '$4 ~ pattern { print $2; exit }')
    test -n "$size" || {
        echo "IPC-INSTRUCTIONS $name: symbol missing" >&2
        exit 1
    }
    bytes=$((0x$size))
    instructions=$((bytes / 4))
    test "$instructions" -le "$limit" || {
        echo "IPC-INSTRUCTIONS $name=$instructions limit=$limit: FAIL" >&2
        exit 1
    }
    echo "IPC-INSTRUCTIONS $name=$instructions limit=$limit: PASS"
}

check_symbol '_ZN3sys6kernel7syscall4call' call 300
check_symbol '_ZN3sys6kernel7syscall7receive' receive 280
check_symbol '_ZN3sys6kernel7syscall15reply_to_caller' reply 240
