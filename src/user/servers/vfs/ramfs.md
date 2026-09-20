# ramfs

The writable half of the VFS: a small, flat, in-memory filesystem serving
/tmp. ext2 is mounted read-only, so redirection and scratch files need
somewhere writable and this is it.

Deliberately flat, with no subdirectories: nothing that needs /tmp needs a
hierarchy under it — a shell redirecting output, or a pipeline's
intermediate file, wants a name rather than a tree. Storage rides on this
process's own `malloc`/`realloc`, which is itself frame-backed, rather than
hand-rolling frame management a second time for what is underneath the same
kind of growable buffer.
