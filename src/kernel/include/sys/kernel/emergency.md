# Emergency diagnostics

Each CPU owns a fixed lock-free 32-record ring. Writers reserve a sequence,
populate a versioned record, and publish it with release ordering. Exception,
IRQ, scheduler, VM-exit, fault, fatal, and printk-contention paths may append
without allocation or ordinary kernel locks.

The crash record resides in `.noinit`, carries a checksum, and survives normal
BSS clearing for external machine-controller collection.
