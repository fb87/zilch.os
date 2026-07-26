# ARM64 Hypervisor Profile 0.2 guest payload

The bounded guest image executes from a 64 KiB stage-2-mapped IPA window. It
starts at guest EL1 with stage-1 translation disabled, builds its own three-level
4 KiB identity map, installs an EL1 vector table, and enables the MMU and caches.

The guest then executes `WFI`. EL2 returns a structured wait exit, injects a
virtual IRQ through `HCR_EL2.VI`, and re-enters the vCPU. The guest IRQ vector
acknowledges the interrupt through HVC and reports timer progress.

Finally, the guest enters EL0. `SVC #0` is handled by the guest EL1 lower-EL
vector and returns a cookie; `SVC #1` requests clean shutdown. EL2 accepts the
run only when MMU/vector, virtual timer, and EL0 report bits are all present.

Stage 2 maps the code/vector pages read-execute and all guest data, stacks, and
page-table pages read-write and execute-never.

## Stage-1 W^X bootstrap policy

The guest stage-1 hierarchy uses 4 KiB L3 page descriptors from the first MMU
transition. IPA pages `0x0000` through `0x3fff` contain the reset path, exception
vectors, EL0 test code, and EL1 test code; they are mapped EL1 read-only and
executable. IPA pages `0x4000` through `0xffff` contain writable data, stacks,
and page tables; they are mapped EL1 read-write with both PXN and UXN set.

Because no writable stage-1 page is executable, the guest enables
`SCTLR_EL1.WXN` together with `SCTLR_EL1.M`. Stage 1 and stage 2 therefore enforce
matching W^X policies independently.

## EL2 bootstrap SCTLR application

During the temporary `HCR_EL2.TVM` bootstrap trap, EL2 decodes the trapped Rt
field from `ESR_EL2`, reads the operand from the saved exception frame, and
applies that exact `SCTLR_EL1` value. EL2 then invalidates EL1 stage-1 TLB and
instruction-cache state, clears the bootstrap TVM trap, verifies the combined
stage-1 plus stage-2 translation, and resumes directly at the post-enable label
with EL1h and DAIF masked.
