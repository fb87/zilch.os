# AMD64 hardening compatibility boundary

AMD64 is compile-only in the 1.0 profile. This module exposes the common
hardening inventory interface without advertising runtime mitigation state.
Adding boot support requires feature detection, policy, readback, and runtime
negative tests before the platform can leave the unsupported list.
