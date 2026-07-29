# Batch 0166: Targeted IPC wakeup and queue ownership

Cross-CPU IPC completion now sends the reschedule SGI only to the receiver's
pinned CPU. On the QEMU ARM64 topology, logical CPU identifiers map directly
to the GICv3 affinity-zero target list used by `ICC_SGI1R_EL1`. Other CPUs no
longer take an unrelated reschedule interrupt for each remote rendezvous.

Endpoint validation now traverses the bounded sender ring and requires every
entry to be a live generation-checked thread reference. It rejects duplicate
senders and a thread appearing simultaneously as sender and receiver. The
receiver reference, when present, must also resolve to a live thread.

Final certification continues to validate every endpoint after the lifecycle
race and teardown workloads. Cross-CPU wake telemetry begins immediately
before the targeted SGI and completes on the receiving CPU, providing
end-to-end evidence for the target path.
