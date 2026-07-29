# ARM64 exception-stack protection

Each of four CPUs owns separate EL1 and EL2 stack slots with an unmapped guard
page below a 32 KiB usable region. Initialization installs canaries and
exception entry validates bounds while retaining low-water marks. Bootstrap
walks guard and adjacent usable PTEs before userspace starts.
