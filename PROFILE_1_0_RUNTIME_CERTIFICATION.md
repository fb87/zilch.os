# ARM64 Kernel Profile 1.0 runtime certification

The certification build retains the deterministic four-CPU workload and emits
machine-readable records. Runtime certification passes only after:

- capability lifecycle, memory mapping/W^X and notification self-tests pass;
- the expected user fault is delivered through fault IPC;
- every online CPU advances operations, scheduler switches and timer ticks;
- at least 1,048,576 fuzz operations complete;
- the fuzz failure count remains zero.

Expected terminal records:

```text
[TEST] name=capability_lifecycle result=PASS
[TEST] name=memory_map_unmap_wx result=PASS
[TEST] name=notification_signal_consume result=PASS
[TEST] name=fault_ipc_delivery result=PASS
[TEST] name=smp_all_cpus_progress result=PASS
[TEST] name=deterministic_fuzz result=PASS
[ACCEPTANCE] profile=1.0 phase=runtime result=PASS operations=1048576 failures=0
[ACCEPTANCE] profile=1.0 result=PASS failures=0
```

This certifies the currently selected compatibility acceptance boot. Root-only
boot from the standalone `init.elf` remains a separate release gate and is not
claimed by this record.
