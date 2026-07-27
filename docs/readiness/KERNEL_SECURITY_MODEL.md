# Kernel security model

## Threat model and trust boundaries

EL1 kernel code, the boot chain, platform firmware, and the compiler/toolchain are
trusted. PL3 tasks, their inputs, guest EL1/EL0 code, guest memory contents, and
all capability selectors and syscall arguments are untrusted. The root task is
trusted to make system policy but receives no implicit kernel-memory authority.
Capabilities are the authority boundary between tasks; stage-2 translation is
the boundary between guests and host memory; exception entry is the boundary
between untrusted execution and kernel control.

The kernel defends confidentiality and integrity across task and VM boundaries,
rejects stale generation-tagged authority, contains user and guest faults, and
preserves diagnostics after kernel failure. Availability against a root task
that intentionally exhausts its delegated quota is a policy concern. Physical
attack, malicious firmware, cache-timing elimination, and DMA isolation before
the SMMU/device subsystem exists are outside the current certified boundary.

## User-memory access

EL1 runs with PAN enabled where supported and UAO disabled. The only kernel copy
primitives are `copy_from_user` and `copy_to_user`. Before touching memory they
reject arithmetic wrap, kernel-boundary crossing, unsupported L2 windows,
unmapped pages, non-EL0 mappings, and writes to read-only pages across every page
in the requested range. They then issue unprivileged `LDTRB`/`STTRB`
instructions, so translation permissions remain EL0 permissions even at EL1.
A CSDB/ISB sequence prevents the validated range from being bypassed through
speculative use.

## Speculation, pointer authentication, and BTI

Every CPU inventories CSV2, CSV3, SSBS, pointer-authentication, and BTI feature
fields. Kernel/user validation boundaries use an architectural CSDB plus ISB,
which is safe as a HINT on older Armv8-A implementations. PAN, UAO, W^X, address
validation, and stage-2 isolation remain the primary architectural controls.

SEC-008 remains incomplete until a real supported platform either advertises
the required CSV properties or supplies a platform firmware mitigation that is
runtime certified. QEMU does not substitute for that hardware evidence.

Pointer authentication and BTI were evaluated for 1.0. They are not enabled in
the 1.0 baseline because exception vectors, guest entry/exit, context switches,
and hand-written boot assembly do not yet have complete signing/landing-pad
coverage. Enabling only compiler-generated C++ would create a misleading partial
control-flow boundary. A later ABI/toolchain revision must enable each feature
kernel-wide, audit every assembly entry, and retain negative landing/signature
tests. This explicit all-or-nothing strategy closes the evaluation requirements
without claiming the mechanisms are active.
