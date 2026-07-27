# ARM64 Kernel Profile 1.0: root-created system certification

The root-only profile creates one initial task. The root task then invokes
`child_create` to construct private Task, CSpace, AddressSpace, Thread, and
SchedulingContext bundles for CPUs 1 through 3. No secondary worker exists
before these root invocations.

Each worker runs the standalone init payload in a worker role and executes a
replayable deterministic negative-control fuzz corpus:

- invalid notification selectors,
- invalid thread-control selectors,
- invalid address-space/frame mapping selectors,
- deletion of an empty capability slot.

The acceptance threshold is 4096 operations on each secondary CPU with zero
unexpected results. Root then suspends and destroys every bundle, recreates the
CPU1 bundle in the same object-table slots, and destroys it again. This verifies
object generation advancement, stale-reference rejection, capability revocation,
and bounded object reuse.

Expected terminal records:

```
[TEST] name=root_created_objects result=PASS
[INFO] root fuzz cpu=1 operations=4096 failures=0 status=PASS
[INFO] root fuzz cpu=2 operations=4096 failures=0 status=PASS
[INFO] root fuzz cpu=3 operations=4096 failures=0 status=PASS
[TEST] name=root_created_smp_fuzz result=PASS
[TEST] name=object_destroy_reuse result=PASS
[ACCEPTANCE] profile=1.0 boot=root-only result=PASS failures=0
```
