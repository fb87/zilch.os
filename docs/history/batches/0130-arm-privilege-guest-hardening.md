# Batch 0130: ARM privilege and guest-control hardening

## Targets

- Complete HYP-043 unknown-hypercall containment.
- Complete SEC-011 PAN/UAO policy where implemented by the CPU.
- Advance SEC-007 and HYP-046 with an enforced SCTLR_EL1 mask.

## Implementation

The EL1 bootstrap reads `ID_AA64MMFR1_EL1`, enables PAN when supported, disables UAO when supported, and validates the resulting PSTATE fields. Encoded instructions preserve the kernel's Armv8-A compiler baseline and are executed only after feature discovery.

The EL2 guest path no longer writes an untrusted SCTLR_EL1 operand directly. It retains supported guest controls, restores architectural RES1 bits, and clears all other bits. Unknown HVC numbers produce a contained host exit and preserve the rejected number in the exit qualification.

## Evidence

Certification tests reject an all-ones unknown hypercall number and validate both all-ones and zero SCTLR inputs. `make BUILD_VARIANT=certification format clean run` passes the four-CPU root-only acceptance suite with zero failures, including real guest MMU execution.
