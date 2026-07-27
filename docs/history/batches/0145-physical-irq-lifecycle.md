# Batch 0145: physical IRQ lifecycle

## Outcome

Non-reserved GIC interrupts now enter an exclusive capability-owned lifecycle
with explicit mask, priority-drop, deactivate, acknowledge, unmask, accounting,
and storm containment semantics.

## Evidence

- `irq_ownership_delegation` registers edge IRQ 40, delegates write-only
  authority into a guarded CSpace, rejects control use, delivers a notification,
  and revokes the descendant capability.
- `irq_ack_deactivate` accepts exactly one acknowledge for each active delivery
  and retains balanced delivery/acknowledge counters.
- `irq_storm_containment` crosses the 64-event window threshold, verifies
  suppression and persistent masking, and completes the outstanding deactivate.
- Existing real timer, reschedule IPI, TLB-shootdown IPI, four-CPU scheduling,
  guest execution, and lock-order certification remain green with GIC EOImode 1.

## Remaining work

External platform-resource discovery, userspace device-manager publication, and
real device edge/level tests remain open. Shared physical lines are explicitly
unsupported for 1.0.
