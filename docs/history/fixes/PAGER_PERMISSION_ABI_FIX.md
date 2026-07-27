# Pager permission ABI correction

The generation-safe mapping database tightened permission validation to require readable mappings. The userspace memory server still supplied the literal value `2`, which requested write-only access. ARM64 stage-1 mappings do not provide a useful write-only permission, so the kernel correctly rejected the pager reply.

The public ABI now defines `memory_permission`, and the memory server explicitly requests `read | write`. Write-only and W+X mappings remain rejected by the kernel.
