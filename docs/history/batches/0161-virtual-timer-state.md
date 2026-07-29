# Batch 0161: guest virtual timer state and counter offsets

This batch closes the basic virtual-timer state and per-VM counter-offset
requirements through the real ARM64 guest path.

- Guest `CNTV_CTL_EL0` and `CNTV_CVAL_EL0` are captured on every EL2 exit and
  restored on the next entry.
- A VM-owned counter offset is programmed into `CNTVOFF_EL2` and read back
  before guest execution.
- Normal exits and rejected entries restore the host HCR, VTTBR, VTCR, and
  counter offset together.
- Production vCPU entry synchronizes timer state, recognizes elapsed
  unmasked deadlines, and queues one virtual timer IRQ until acknowledgement.
- The certification guest arms a far-future timer before WFI and resumes with
  a nonzero VM offset. The host validates the retained control and compare
  values.
- Deterministic certification covers pre-deadline behavior, one-shot expiry,
  duplicate-expiry suppression, acknowledgement, re-arm, and cancellation.

HYP-034 through HYP-037 are complete. HYP-038 through HYP-040 remain in
progress because deadline-driven wakeup, real cross-CPU migration, and
concurrent expiry/cancellation stress are not yet implemented.
