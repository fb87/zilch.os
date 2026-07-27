# Batch 0124: atomic capability lookup snapshots

Capability lookup previously read a live CSpace slot without its lock. A
concurrent delete or revoke could therefore expose fields from different slot
generations to one lookup.

Both pointer-returning and slot-returning lookup paths now hold the CSpace lock
while they:

- copy the slot;
- validate object type and derivation activity;
- validate required rights;
- resolve the generation-tagged object reference.

This supplies a coherent lookup linearization point against slot mutation. It
does not yet provide a lease that keeps the resolved object alive after lookup;
object-use quiescence remains a separate production gate.

Evidence:

- certification and release builds pass;
- four-CPU certification acceptance passes with zero failures;
- the SMP fuzz and object destroy/reuse suites pass.
