# ext2 reader

Read-only traversal: mount, resolve a path, read file bytes, list a
directory. No dynamic allocation and no recursion — every buffer is caller-
or stack-local and fixed, and path resolution is an iterative walk bounded
by the length of the path string itself.

I/O is abstracted to a single callback reading a fixed 1024-byte unit. That
size is not arbitrary: the superblock sits at byte offset 1024 regardless of
the filesystem's own block size, so mounting must read in 1024-byte units
before the block size is even known. Every ext2 block is then read as one,
two, or four such units, which keeps exactly one I/O primitive for the whole
reader rather than a bootstrap-only one plus a block-sized one.
