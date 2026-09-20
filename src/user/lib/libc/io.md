# File descriptors, processes, and console I/O

The descriptor table maps small integers onto either the console or a
server-side VFS handle. `dup2` copies an entry, which is how redirection
works, and `execv` carries a redirected handle across image replacement as
internal environment entries because the table itself is process memory the
new image does not inherit.

`fork` reuses the lowest free child slot, and resets the cached
"VFS frame is mapped" flag in the child: that cache is ordinary process
memory the child inherits as true, but it says nothing about whether the
child's own page table entry still points at the shared frame rather than a
private copy.

`waitpid` polls `process_wait`, which returns `busy` until the child exits —
this kernel has no blocking wait — and yields between attempts rather than
spinning on the global authority lock. `process_reap` then tears down the
bundle; it also deletes the parent's capability, so nothing further is
needed here.
