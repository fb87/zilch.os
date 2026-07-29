# Process service

Role `0x200` owns process-image and lifecycle policy. The current vertical slice
validates its bounded quota/restart policy and participates in the real PL3
service graph. Path-based ELF loading and crash-report IPC remain open.
