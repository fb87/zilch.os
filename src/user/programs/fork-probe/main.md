# fork probe

Exercises fork, exec and wait end to end: writes a value before forking and
re-checks it in the child to prove the address space was copied rather than
shared, execs `exec-probe` to prove image replacement works, and waits for
the exit status.

Uses two distinct child selectors so the second fork does not have to wait
for the first child's slot to be released. That detail matters for anyone
reading it as a model — it is why this probe passed while the shell, which
reuses one selector, did not.
