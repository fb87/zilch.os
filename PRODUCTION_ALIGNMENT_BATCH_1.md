# Production Alignment Batch 1

This batch does not claim production readiness. It establishes mandatory product/test separation and release gates.

## Implemented

- `BUILD_VARIANT=development|certification|release`.
- Self-test control operations use a private high-numbered test range and compile only in certification builds.
- Hypervisor fuzz/self-test operations compile only in certification builds.
- Release userspace contains no certification harness.
- `production-gate` builds a release image and rejects embedded acceptance/self-test markers.
- `release` runs the production gate before packaging.

## Evidence boundary

A successful build proves only source/build separation. It does not prove capability, IPC, RT, userspace-server, multi-vCPU, SMMU, hardware, or soak requirements.

## Progress states

- `implemented`: production code is present and build-reviewed.
- `verification-pending`: implementation is complete but external runtime/hardware evidence is not yet attached.
- `verified`: required runtime, stress, failure, and hardware evidence is retained.
- `certified`: all release gates for the requirement pass.

Implementation completion never implies verification or certification.
