# Capability and IPC lifecycle batch 0090

This batch hardens the existing L4 capability and synchronous IPC foundations without claiming the full CAP-GATE or IPC-GATE.

## Capability changes

- derivation records are reusable rather than monotonically exhausted;
- lookup rejects inactive or object-mismatched derivation records;
- mint uses the explicit fifth control argument as its badge;
- revoke operates on descendants of the selected derivation across registered CSpaces;
- revoke requires grant or manage authority;
- certification performs 128 mint/copy/revoke/reuse cycles and rejects rights escalation.

## IPC changes

- direct call-to-waiting-receiver performs capability transfer exactly once;
- failed direct transfer restores the receiver registration and leaves the caller runnable;
- successful transfer remains atomic because the destination slot is installed before either thread is exposed to user execution;
- reply authority remains a protected, generation-checked, single-use kernel slot bound to the receiving thread.

## Remaining limitations

- the CSpace remains flat and bounded;
- derivation records are a bounded table rather than a scalable CDT;
- capability transfer supports one capability per message;
- receiver destination-window negotiation is not yet implemented;
- timeout/cancel/revoke race stress is not yet exhaustive;
- reply authority is kernel-internal rather than a first-class reply object.
