# Certification control wrapper

This header is available only to certification userspace builds. It sends
operations from the separate `sys::test_abi` namespace through the raw control
syscall. Production userspace include paths do not expose this wrapper or the
test ABI.
