# AMD64 stack compatibility boundary

The AMD64 header preserves the architecture-neutral stack-observation API for
compile compatibility. Runtime stack guards, exception-time bounds, canaries,
and high-water evidence are certified only on ARM64 in 1.0.
