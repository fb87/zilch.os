# ARM64 CPU hardening inventory

Every online CPU records CSV2, CSV3, SSBS, pointer-authentication, and BTI
feature fields before userspace starts. Privilege-boundary validation executes
CSDB followed by ISB. Final acceptance requires inventory publication for the
complete immutable CPU set.

PAuth and BTI remain deliberately disabled until every C++ and assembly entry,
exception, context-switch, and guest path can adopt and negatively test them
together.
