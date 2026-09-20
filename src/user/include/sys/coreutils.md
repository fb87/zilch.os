# Coreutils role table

One table, shared by the two things that must never disagree about it:
root, which binds each coreutil's role at boot, and libc's `execv`, which
resolves a program name against it. A second copy would drift.

Each entry is a name, a role identifier, and an earlyfs image path. `execv`
resolves only these fixed names — it does not search a path or load bytes
from a file, because `process_exec` selects an already-bound role rather
than binding one, which is what lets an ordinary forked child exec a
coreutil without holding the root-gated right to bind. Loading an arbitrary
program from disk is unimplemented, and deliberately so.
