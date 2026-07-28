# Batch 0152: sporadic budgets and lock evidence

This batch was an experimental scheduler-budget change. Its per-slice runtime
integration was reverted after certification exposed a fault-service liveness
regression; the stable periodic model remains in production.

- The attempted sporadic queue and its certification probe are retained as
  design history, not as production evidence.
- The stable periodic budget model and existing lock-hold measurement remain
  the certified behavior.
- SCH-010 and SCH-017 remain open.
