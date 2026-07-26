# Guest stage-1 translation probe

Profile 0.2 now probes VA `0x2000` with `AT S1E1R` before enabling `SCTLR_EL1.M`.

Markers:

- `T`: bootstrap tables built.
- `C`: translation registers installed and local EL1 TLB invalidated.
- `F`: `PAR_EL1.F` reported a translation failure before MMU enable.
- `P`: the pre-enable translation probe succeeded.
- `M`: translated execution continued after enabling `SCTLR_EL1.M`.
- `K`: instruction and data caches were enabled successfully.

The guest enables translation first and caches in a separate step so failures are attributable to one boundary.
