# Explicit user-memory access

User-copy helpers validate arithmetic, the complete page range, EL0
accessibility, requested write permission, and the kernel boundary. ARM64 then
uses unprivileged LDTRB/STTRB accesses so EL0 translation permissions remain
authoritative at EL1. CSDB+ISB separates validation from use.
