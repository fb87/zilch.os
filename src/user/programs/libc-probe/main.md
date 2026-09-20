# libc probe

Boot-time self-check of the freestanding libc: string and memory
primitives including the overlapping `memmove` case a forward copy
corrupts, formatted output including width and length modifiers, numeric
parsing, and heap allocation across a quota boundary.

Runs as a real process rather than a unit test because the properties that
matter here — the heap is frame-backed and quota-charged, `printf` reads
varargs under AAPCS64 — only hold in a real address space.
