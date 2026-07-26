# Kernel Profile 1.0 objects

The profile initializes the bounded boot object set used to bootstrap the root task: frames, page tables, notification and timer-interrupt authority. Production systems replace bounded pools with untyped-memory retyping while preserving the same object and capability contracts.
