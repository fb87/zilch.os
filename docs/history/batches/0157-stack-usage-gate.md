# Batch 0157: release stack usage and packaging gate

Compiler stack-usage records are emitted for freestanding C++ objects. The
production and release gates scan both supported release outputs and fail if
any function exceeds 8 KiB, leaving headroom within the 32 KiB per-CPU stack.
The release packager now consumes the actual `out/release/{arm64,amd64}`
layout used by the build, so the complete release target can finish after the
architectural checks.
