# Environment

`environ` points at whatever the spawner placed in the argument block; the
runtime installs it before `main` runs. A program started with no block gets
a null `environ`, which `getenv` treats as an empty environment rather than
faulting.

`getenv` matches the name followed by `=`, not a bare prefix — comparing the
prefix alone would let `PATHOLOGICAL=1` answer a lookup for `PATH`.
