---
module: sys.kernel.printk
layer: kernel
status: bringup
sources:
  - printk.hh
---

# Kernel logging

`printk` is a freestanding C `va_list` formatter writing directly to the selected
platform console. A raw SMP spinlock serializes complete records so characters
from different CPUs cannot interleave. Local IRQ state is saved and IRQ delivery
is disabled while the lock is held, then restored exactly to its previous state.

RT and exception paths use `printk::defer(event, arguments...)` instead of the
formatter. It publishes one bounded structured record to the current CPU's
lock-free emergency ring without taking the console lock, disabling interrupts,
allocating, or depending on scheduler state. Formatting and draining those
records remains a separate diagnostics-service responsibility.

The lock is intentionally scheduler-independent and allocation-free. Logging
while recursively entering `printk` on the same CPU is unsupported and must be
avoided in fatal low-level paths.
