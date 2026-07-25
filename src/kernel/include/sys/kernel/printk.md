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

The lock is intentionally scheduler-independent and allocation-free. Logging
while recursively entering `printk` on the same CPU is unsupported and must be
avoided in fatal low-level paths.
