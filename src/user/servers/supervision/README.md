# Supervision service

Role `0x204` starts after all other control-plane roles. Root collects the
collision-free readiness mask and retains process capabilities for controlled
suspend/destroy. The management ABI supports orderly stop, atomic exit badges,
bounded restart admission, bundle recreation, endpoint reminting, and health
revalidation. Unexpected-fault status, restart backoff, and time-windowed
crash-loop containment remain open.
