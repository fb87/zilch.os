# Shell

An interactive shell over the console, and the first program to exercise
fork, exec, wait, argv/environment and VFS file I/O together as an ordinary
process rather than a boot-time probe.

Scope is stated rather than discovered. Commands resolve against the fixed
coreutils table, not a path search. A pipeline runs its stages
*sequentially* through a ramfs temp file — this kernel has no pipe object —
so `a | b` behaves as `a >/tmp/.pipeN; b </tmp/.pipeN`: correct output, no
streaming overlap. Line editing is backspace and Ctrl-C, which cancels the
line rather than the process, because there is no signal delivery to
interrupt a running child with.
