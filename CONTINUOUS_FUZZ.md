# Continuous Profile 1.0 fuzzing

The finite certification profile remains the default and terminates after the
root-created SMP workload, teardown, and object-generation reuse checks pass.

Build and run the finite suite:

```sh
make run-certify
```

Build and run continuous soak fuzzing:

```sh
make run-soak
```

`FUZZ_MODE=soak` first executes the complete finite Profile 1.0 certification.
Only after the final acceptance PASS does root recreate workers on CPUs 1-3 and
enter an unbounded deterministic negative-control fuzz loop.

Every worker executes the same four-case corpus with a reproducible xorshift
state. A soak epoch requires 65,536 additional operations on every worker CPU.
The kernel prints one record per CPU:

```text
[SOAK] epoch=1 cpu=1 operations=... failures=0 seed=... progress=yes
[SOAK] epoch=1 cpu=2 operations=... failures=0 seed=... progress=yes
[SOAK] epoch=1 cpu=3 operations=... failures=0 seed=... progress=yes
```

If any worker reports an unexpected result, root prints the failing CPU,
operation count, failure count, and last deterministic PRNG state, then stops
advancing the test so the record can be replayed.

Continuous mode never exits successfully; terminate QEMU externally after the
desired soak interval. It is intentionally separate from finite CI.
