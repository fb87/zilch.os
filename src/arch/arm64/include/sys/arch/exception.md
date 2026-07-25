# Module: ARM64 exceptions

## Purpose

Defines the C++ exception-frame contract and installs the EL1 vector base.

## Bridge contract

`vectors.S` only selects a vector number, preserves the interrupted general-purpose
register state, calls `sys_arm64_exception_handler()`, restores state, and executes
`eret`. Syndrome decoding and IRQ policy remain in C++.

## Vector coverage

Separate 2 KiB-aligned tables are provided for EL1 and EL2, with all sixteen ARM64
vector slots present in each table.
