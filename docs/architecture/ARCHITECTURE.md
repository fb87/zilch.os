# Architecture

Status: describes what is implemented on the QEMU ARM64 1.0 platform
profile. Where something is bounded, unimplemented, or true only of this
platform, it says so rather than describing an intention.

This is the map. The territory is in the documents it points at, which are
maintained alongside the code they describe.

## Shape

An L4-style capability microkernel. The kernel provides address spaces,
threads, synchronous IPC, capabilities, and physical interrupt routing.
Everything a system normally calls an operating system — device drivers, a
filesystem, process spawning, service supervision, a VMM — runs as ordinary
unprivileged processes that hold capabilities.

Privilege layout on ARM64:

| Level | Runs |
| --- | --- |
| EL2 | Hypervisor extensions: stage-2 translation, vCPU entry/exit |
| EL1 | The kernel |
| EL0 | Everything else, including root |

The kernel owns EL2 rather than delegating it, so a guest VM's exits are
serviced by the same kernel that schedules its VMM.

## Implementation form

The kernel is header-only apart from a single translation unit
(`src/kernel/kernel.cc`, which holds the entry points and the idle loop);
the implementation lives in ~43 headers under
`src/kernel/include/sys/kernel/`. That is a deliberate structure, not an
accident of growth — module boundaries are enforced by review and by
`tools/doc/collect.py`, which requires a module document beside each
header and is wired into `make doc-check`.

Architecture-specific code is confined to `src/arch/<arch>/` and platform
specifics to `src/platform/<platform>/`. Userspace cannot include either:
services that need device register layouts restate them locally, which is
why the serial driver carries its own PL011 offsets.

## Authority

There is no ambient authority anywhere. A thread can act on an object
exactly when a capability to it occupies a slot in its CSpace that it can
name. There is no namespace, no lookup by name, and no way to widen rights:
derivation may only attenuate.

This extends to hardware. A device driver holds a frame capability for its
MMIO window and an interrupt capability for its line, both minted by root
before the driver runs; revoking either withdraws real access, not just the
name — the capability layer calls a teardown hook that masks and
deactivates the line or tears down the frame's mappings.

Read `readiness/CAPABILITY_IPC_SEMANTICS.md` for rights, generation
checking, the syscall register convention, and the full IPC contract;
`readiness/CSPACE_DESIGN.md` for selector geometry and capacity;
`readiness/IPC_BADGE_PROTOCOL.md` for caller identity.

## Everything is bounded

Every kernel *object* lives in a fixed-size pool sized at compile time, and
exhaustion returns an error rather than growing or evicting. There is no
general-purpose allocator: the one thing that is genuinely allocated is
physical pages, drawn from the discovered memory range and charged against
a per-task quota.

| Resource | Bound |
| --- | --- |
| User threads / tasks / address spaces | 16 |
| CPUs | 4, and this is a hard requirement — see below |
| CSpace slots per CSpace | 256 |
| Registered CSpaces | 32 |
| Derivation records / maximum depth | 4095 / 64 |
| Static / dynamic endpoints | 2 / 16 |
| Capabilities per IPC transfer | 4, all-or-nothing |

Objects are generation-tagged, so a capability naming a destroyed object
fails closed rather than addressing whatever later reused the slot. That is
what makes recycling a bounded pool safe.

**SMP width is not configurable.** The kernel halts at boot unless it can
start all four CPUs; `CPUS=1` and `CPUS=2` both stop at `secondary CPU
startup failed`. "Runs on arm64" currently means "runs on a 4-CPU arm64".
See readiness checklist 0157.

## Concurrency

Kernel locks are ticket locks under a single global rank order
(`readiness/LOCKING_PROTOCOL.md`). Fairness is a correctness requirement
rather than tuning: the kernel has no blocking wait, so callers of
`process_wait` and `notification_poll` poll, every poll resolves a
capability and takes the global authority lock, and an unfair lock lets a
poller starve the thread it is waiting for. A lock word must never be
reset, zeroed, or rewound.

Syscalls run to completion; the IRQ path reprograms the timer and returns
without switching threads, so nothing is preempted holding a ticket.

## Scheduling

A fixed pool of 16 threads across 4 CPUs, each thread pinned. Priorities
with bounded sporadic scheduling contexts, per-CPU timeout queues, and
scheduling donation across IPC — a caller's context is donated to the
server for the duration of a reply, so a high-priority caller does not
queue behind the server's own priority.

`thread_create` gives each sibling thread its **own address space** and
shares only the CSpace. A driver whose second thread touches device
registers must map them again in that thread's space; this has caught
people twice.

## Memory

Physical memory is discovered from the device tree, with a platform probe
and a fallback. Frames are capability objects; mappings are per address
space and torn down with it. Per-task memory quotas are set at process
construction and enforced for resource-backed objects.

ASIDs are a recycled tagged pool with rollover that preserves tags still
installed on a CPU. A page cannot be freed while a live address space still
maps it — an allocator barrier checks, and the self-teardown case is
distinguished from theft by an explicit release context, since a space
handing back its own pages is legitimate and looks identical otherwise.

## The userspace service graph

Root (`src/user/servers/root`, driven by `sys::root_graph`) launches and
supervises the graph, holding the root-gated authority to create device
frames, interrupts, and processes. Each service is independently linked and
reached only through a capability its clients were given before they ran;
the capability-slot convention stands in for name lookup.

Services: `console` (multiplexes clients onto the serial driver),
`serial` and `virtio` drivers, `vfs`, `memory`, `process`, `device`,
`domain` (the VMM), `supervision`, `control_plane`. Programs — a shell and
a fixed coreutils set — are ordinary processes on top.

Roles publish readiness and exit through badge bits on a notification root
polls; a role that faults *or* exits cleanly is restarted through bounded
admission control. Wire protocols are in
`readiness/USERSPACE_SERVER_APIS.md`.

## Hypervisor

VMs and vCPUs are capability objects. Stage-2 mapping takes a guest IPA and
a frame capability — userspace can never submit a physical address — with
W^X derived from ELF section flags at load time. The guest image is parsed
and loaded entirely in userspace; the kernel supplies mapping mechanism and
has no notion of an executable format.

What a guest sees, and the substantial list of what it does not (no
guest-facing PSCI, no GIC distributor emulation, no constructed device
tree, no storage), is in
`readiness/HYPERVISOR_GUEST_ARCHITECTURE.md`.

## Build profiles

Kconfig-generated, selected by `KCONFIG_DEFCONFIG` against
`configs/{debug,release,guest}_defconfig`. Release makes every test and
diagnostic option unavailable rather than merely off — `CONFIG_TESTS`,
`VERBOSE_DIAGNOSTICS`, `TRACE` and `DEBUG_INFO` all depend on
`BUILD_DEBUG`. A `BUILD_VARIANT` mechanism still coexists with the
defconfigs (PRD-020).

`make smoke` boots the profiles the in-kernel certification suite
structurally cannot cover, since that suite replaces init's `main()`.
`make boot-repeat` cold-boots repeatedly, which is the only way to see a
timing-dependent boot stall.

## Reading order

Start with `readiness/CAPABILITY_IPC_SEMANTICS.md` — nothing else makes
sense before the authority model does. Then
`readiness/USERSPACE_SERVER_APIS.md` for how the system is actually
assembled, and `readiness/PRODUCTION_READINESS_CHECKLIST.md` for what is
genuinely finished, what is bounded by design, and what is known broken.
That checklist is authoritative where this document and it disagree.
