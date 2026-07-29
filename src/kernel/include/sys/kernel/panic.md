# Scheduler-independent panic path

Fatal handling masks exceptions, writes the CPU-local emergency ring and the
checksummed `.noinit` crash record, then stops without consulting scheduler,
allocator, capability, object, or console-lock state. Certification poisons
scheduler identity and holds printk locked while validating capture.
