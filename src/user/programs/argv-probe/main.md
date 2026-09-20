# argv probe

Verifies that argv and envp survive the spawner's argument-block encoding:
that argc matches, that each argument arrives byte-for-byte, and that the
environment is readable through `getenv`.

Exists because the argument block is the one path by which a spawner's
intent reaches a new program, and a silent truncation there would surface
much later as a program misbehaving on input nobody suspected.
