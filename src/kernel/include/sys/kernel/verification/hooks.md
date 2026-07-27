---
module: sys.kernel.verification.hooks
---

Production verification hooks are intentionally no-ops. Certification builds
shadow this interface from `tests/include/` with the retained evidence harness.
Production mechanisms call this narrow interface and never depend on acceptance
state, counters, result formatting, or certification policy.
