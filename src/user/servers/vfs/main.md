# VFS server

Serves the `vfs_operation` protocol over a capability-protected endpoint,
dispatching each path to either the read-only ext2 reader or the writable
ramfs at /tmp.

Handles are server-side state keyed to the calling process rather than to
the running image, which is what lets a redirected descriptor survive
`execv`: libc passes the handle number across image replacement and the new
image's runtime reinstalls it, with no reopen by path.

When no block image is present the ext2 mount is simply absent and /tmp
still works, so the shell and its redirection remain usable on a machine
with no disk.
