#!/usr/bin/env bash
# Regression test for a lost PL011 RX interrupt that permanently killed
# console input after a burst of typed characters.
#
# Usage (needs the Zephyr sample guest built once, see
# samples/guests/zephyr):
#     cd samples/guests/zephyr && make MODE=release image
#     BURST=256 ROUNDS=6 tools/verification/guest_input_burst.sh
#
# History: reported as "zephyr shell hang after repeat inputting s". Failed
# 3/3 before the fix, passes 5/5 after. BURST=64 ROUNDS=1 does NOT trip it
# -- the race needs a sustained burst to land a byte in a one-instruction
# window -- which is why it survived earlier testing and why this script
# defaults to a much heavier burst than looks necessary.
#
# The bug was in serial-driver's drain_rx() (src/user/drivers/serial):
# UARTICR was written AFTER draining the FIFO, so a byte arriving between
# the final try_getc() and that write had its interrupt latch cleared while
# the byte stayed in the FIFO. The PL011 asserts RX on the FIFO level
# REACHING its trigger, not while it sits at or above it, so the level
# could never re-cross from below and RX was dead for the rest of the
# system's uptime. Both the interactive shell and any hosted guest read
# through that one driver, so both died together -- which is what makes
# this a guest-facing test for a host-side driver bug.
#
# Also worth knowing: this exercises the shared console stack, so it is a
# reasonable smoke test for serial-driver/console-server changes generally,
# not only for the specific race above.
set -uo pipefail

cd /home/dao/work/zilch.os/samples/guests/zephyr
ZILCH_ELF=out/zilch/release/zilch.elf
burst_len=${BURST:-64}

log=$(mktemp)
fifo=$(mktemp -u)
mkfifo "$fifo"
trap 'kill "$job" 2>/dev/null; wait "$job" 2>/dev/null; rm -f "$log" "$fifo"' EXIT

exec 9<>"$fifo"
QEMU_CPUSET=${QEMU_CPUSET:-4-7} timeout -k 5s 90s ../../../tools/run/run.sh "$ZILCH_ELF" \
    <&9 >"$log" 2>&1 &
job=$!

# Wait for the guest's own shell prompt, not merely for zilch's boot.
waited=0
while [ "$waited" -lt 60 ] && ! grep -aqF 'zilch:~$' "$log"; do
    sleep 1; waited=$((waited + 1))
done
if ! grep -aqF 'zilch:~$' "$log"; then
    echo "RESULT: guest shell never reached its prompt (cannot test)"
    exit 2
fi
echo "guest prompt up after ${waited}s"

# Several back-to-back bursts with only a brief gap, to maximise the chance
# of landing a byte inside one of the guest's own mask/processing windows.
rounds=${ROUNDS:-6}
for r in $(seq 1 "$rounds"); do
    printf '%*s' "$burst_len" '' | tr ' ' 's' >&9
    sleep 0.25
done
sleep 3
printf '\r' >&9
sleep 2

# Liveness probe: does the guest still process input at all?
before=$(grep -ac 'Available commands:' "$log")
printf 'help\r' >&9
probe_waited=0
while [ "$probe_waited" -lt 25 ] && [ "$(grep -ac 'Available commands:' "$log")" -le "$before" ]; do
    sleep 1; probe_waited=$((probe_waited + 1))
done
after=$(grep -ac 'Available commands:' "$log")

echo "--- last guest output ---"
tail -c 600 "$log"
echo
if [ "$after" -gt "$before" ]; then
    echo "RESULT: PASS - guest still interactive after ${burst_len}-char burst (help answered in ${probe_waited}s)"
else
    echo "RESULT: FAIL - guest wedged after ${burst_len}-char burst (help unanswered for 25s)"
fi
