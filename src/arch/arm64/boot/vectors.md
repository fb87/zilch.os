# Module: ARM64 vector bridge

## Purpose

Provides EL1 and EL2 exception-vector tables and the minimum assembly bridge to C++.

## Responsibilities

- Provide all sixteen architectural vector slots for EL1 and EL2.
- Preserve and restore x0-x30 around the C++ handler.
- Pass the exception frame, vector index, and active exception level to C++.
- Return with `eret` after C++ completes handling.

Exception classification, syndrome decoding, interrupt acknowledgement, timer handling,
and failure policy are not implemented in assembly.
