# Batch 0156: UBSan portable ABI gate

The host ABI/layout test now has a dedicated undefined-behavior sanitizer
build and execution gate. `make ubsan-check` compiles `tests/abi/layout.cc`
with Clang UBSan and `-fno-sanitize-recover=undefined`, then runs it with
`UBSAN_OPTIONS=halt_on_error=1`. TST-033 is complete for portable ABI code;
freestanding architecture-specific kernel code remains outside this scope.
