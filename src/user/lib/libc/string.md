# String and memory primitives

Byte-at-a-time implementations, deliberately not word-optimised. Nothing
here sits on a measured hot path — the certified latency bounds are
dominated by IPC round trips, not string handling — so correctness under
`-Wconversion` is worth more than cycles nothing is waiting on.

`memmove` handles overlap in both directions; a plain forward copy corrupts
the overlapping case, which `libc-probe` checks at boot.
