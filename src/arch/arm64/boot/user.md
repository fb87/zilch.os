# Module: user

## Purpose

Defines the page-aligned, position-independent ARM64 EL0 test image used during
initial user-mode bring-up.

## Interface

The image receives a task identifier in `x0` and a delay factor in `x1`. It
loops forever and performs only `svc #0` using the native `sys_ipc` ABI to the
debug endpoint.

## Safety and isolation

The image contains no MMIO access and cannot address kernel memory. It is mapped
read-only and executable in each private task address space.
