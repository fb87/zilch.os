# Source layout

Product code, module documentation, and module tests are colocated.

- `module.hh`: restricted C++ interface and inline implementation
- `module.cc`: linkage anchor
- `module.S`: preprocessed assembly
- `module.md`: design and verification record
- `module.tt`: C++ module test translation unit

The stable C ABI is isolated under `include/abi/`; restricted C++ contracts live under `include/sys/`.
