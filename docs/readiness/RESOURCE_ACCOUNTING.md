# Resource accounting

The kernel maintains allocation-free counters at the two authoritative lifecycle
boundaries.

The object table counts live, peak, created, and destroyed objects by object
type. Registration increments a live/create pair while holding the table lock;
successful unregister decrements live and increments destroyed after the
read-side grace period. A nonzero accounting-fault count or a mismatch between
those totals makes the accounting state invalid.

Each VM counts current and peak stage-2 mapped pages, map and unmap operations,
active vCPUs, and guest run entries and exits. Mapping sizes are page aligned, so
page charges are exact. Reset and teardown clear current charges but retain
cumulative and peak evidence. Saturation and underflow are rejected and retained
as an accounting fault.

A 64-record VM audit ring is always present in production builds. Records use a
monotonic release-published sequence and identify reset, map, unmap, run entry,
run exit, pause, resume, stop, and teardown actions. The ring is bounded and
overwrites its oldest records; durable export is userspace policy.

Certification registers and unregisters a dynamic object, maps and unmaps guest
pages, runs a real guest, and checks that all corresponding live and entry/exit
balances close. Long-duration no-growth soak remains a separate release gate.
