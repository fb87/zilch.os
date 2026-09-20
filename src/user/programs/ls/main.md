# ls

Lists a directory through the VFS `readdir` operation, one entry per call
with a caller-supplied index. The server re-walks the directory on each
call rather than holding a cursor, which is affordable at earlyfs scale and
avoids per-client iteration state that would need invalidating on every
mutation.
