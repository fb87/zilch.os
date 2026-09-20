# VFS probe

Verifies the VFS server end to end from a client's position: open, read,
write, stat, seek, readdir and close, across both the read-only ext2 mount
and the writable ramfs at /tmp.

Checks the shared-buffer convention as much as the operations themselves,
since path and bulk bytes travel in a 4096-byte frame while only lengths and
handles travel in message registers — a client that maps that frame
incorrectly sees plausible-looking corruption rather than an error.
