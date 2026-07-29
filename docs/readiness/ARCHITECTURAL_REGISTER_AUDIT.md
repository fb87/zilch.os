# ARM64 architectural register audit

## Host EL1 controls

| Register | Production policy | Runtime evidence |
|---|---|---|
| `MAIR_EL1` | Attr0 `0xff` normal write-back memory; Attr1 `0x04` device nGnRE; all unused attributes zero | Exact readback in `architecture_hardening_invariants` |
| `TCR_EL1` | 48-bit TTBR0 VA, 4 KiB granule, inner-shareable write-back walks, TTBR1 disabled, 40-bit physical size | Exact readback in `architecture_hardening_invariants` |
| `SCTLR_EL1` | M, C, I, and WXN required; EE and E0E clear | Masked readback in `architecture_hardening_invariants` |
| `PAN` | Enabled when implemented | Feature-detected readback |
| `UAO` | Disabled when implemented | Feature-detected readback |

Page-table validation separately requires kernel text RX, rodata and embedded
images RO-NX, data/BSS RW-NX, and unmapped EL1/EL2 stack guards.

## Guest EL1 controls

Trapped `SCTLR_EL1` writes retain only the supported control mask and restore
the architectural RES1 mask. Certification submits all-zero and all-one
hostile values before the real guest enables its MMU. Stage-2 VTCR/VTTBR/HCR
state is built from constants in the architecture backend and restored on
every exit.

## Scope

CSV2/CSV3/SSBS/PAuth/BTI fields are inventoried per CPU. PAuth and BTI remain
deliberately disabled until all C++ and assembly entry paths can adopt them
together. Real hardware and firmware mitigation qualification remains a
separate release gate.
