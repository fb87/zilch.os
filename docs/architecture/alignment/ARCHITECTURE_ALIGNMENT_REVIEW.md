# Zilch Codebase Alignment Review

## Executive assessment

The current tree is a strong ARM64/QEMU bring-up and validation kernel, but it is not yet aligned with the original production architecture in several essential areas. The core mechanisms—privileged entry, SMP bring-up, capability-checked objects, user-mode root task, IPC entry, stage-1/stage-2 translation, and real EL2 guest execution—are credible. However, much of Profiles 0.3–0.6 is deterministic in-kernel modeling rather than real guest execution, and the userspace control-plane OS remains mostly skeletal.

Current maturity estimate:

- ARM64 kernel bring-up: 70%
- L4 microkernel semantics: 40%
- User-space system architecture: 15%
- Hypervisor real execution: 35%
- Production hardening and verification: 20%
- Overall production-goal alignment: approximately 30–35%

## Original goals used for comparison

1. Small L4-style control-plane kernel.
2. All system policy and services in userspace, including init, memory, process, device, domain, and supervision servers.
3. Capability-authorized access to kernel objects and resources.
4. Native `sys` ABI with stable low-level userspace bindings.
5. ARM64 first, architecture/platform separation, AMD64 as a real second architecture eventually.
6. MMU and interrupts enabled; user processes and servers run at PL3.
7. Real-time scheduling and RT-safe kernel logging from day one.
8. Host Linux, BSD, and Zephyr guest domains; the OS itself remains a small management/control plane.
9. Production-correct lifecycle, isolation, diagnostics, testing, and recovery.
10. Multiple user-space personalities and utility suites are optional ports, not kernel dependencies.

## What aligns well

### Architecture and naming

- Project uses `zilch` consistently.
- Native API uses the `sys` prefix.
- Generic privilege-level terminology is largely architecture-neutral.
- ARM64-specific EL registers and frame handling are confined to the ARM64 backend.
- Architecture, platform, kernel, and userspace trees are separated.
- Freestanding C++20 restrictions are enforced with exceptions, RTTI, hosted runtime, and implicit runtime support disabled.

### Core kernel mechanisms

- Root task runs in user mode and exercises real syscalls.
- ARM64 MMU, exception vectors, GIC, timer, and four-CPU bring-up work under QEMU.
- Object references include generations, reducing stale-reference reuse.
- Capability slots carry object references and rights.
- Kernel supports tasks, threads, address spaces, endpoints, notifications, interrupts, frames, page tables, scheduling contexts, VMs, and vCPUs.
- Object destruction and reuse have runtime tests.
- Stage-1 and stage-2 W^X are tested.
- Real guest EL1/EL0 execution, guest vectors, virtual timer injection, and SVC handling work.
- Negative tests distinguish expected errors from unexpected kernel errors.

### Build discipline

- `-ffreestanding`, `-fno-exceptions`, `-fno-rtti`, `-fno-common`, strict warnings, and `-Werror` are good foundations.
- ARM64 root and compatibility builds plus AMD64 compatibility builds are continuously checked.
- The kernel avoids dynamic allocation in early mechanisms.

## Major misalignments and risks

### Critical: Profiles 0.3–0.6 overstate real hypervisor capability

Profiles 0.3–0.6 primarily execute bounded C++ models inside `kernel/hypervisor.hh`. They do not run multiple independent guest vCPUs through EL2 on separate physical CPUs. Profile 0.6 explicitly documents that real simultaneous secondary-core `enter_guest` calls remain future work.

Consequences:

- “guest SMP online” is a model assertion, not guest code running on four vCPUs.
- “physical CPU lanes” are modeled ownership records, not physical CPUs independently entering guests.
- migration is counter/state transition testing, not save on one CPU and restore/execute on another.
- concurrent multi-VM execution is interleaved model execution, not concurrent EL2 execution.

Action: rename these as control-plane model profiles or replace them with real hardware-execution tests before using “certified” for production claims.

### Critical: Userspace control-plane OS is mostly absent

The userspace tree contains about 395 implementation lines, with roughly half in a self-test `init`. Server directories contain README placeholders rather than working services.

Missing working services include:

- root resource allocator
- physical memory server
- process/program loader
- pager/fault server
- device/resource manager
- interrupt broker
- domain manager/VMM
- supervision/restart service
- console server and userspace UART driver
- storage/root filesystem service

The current `init` is an acceptance-test runner, not the intended management-domain initializer.

Action: freeze kernel feature growth and implement the userspace boot graph.

### Critical: Kernel contains policy and test machinery that should not be ABI

`control_operation` exposes acceptance reports, worker ticks, acceptance queries, child-create test helpers, hypervisor self-test, and fuzz operations as native ABI calls. This couples the production ABI to the certification harness.

Action:

- move acceptance/fuzz operations behind a debug-only test ABI or dedicated test kernel configuration;
- define production object invocation operations separately;
- prevent test selectors and object IDs from becoming ABI commitments.

### Critical: Memory management is a fixed fixture, not a resource model

The kernel memory manager statically provides four frames and four page-table objects. Frames have a single `mapped` boolean and can be mapped only once. There is no:

- boot memory-region ingestion
- untyped-memory/resource delegation
- page-table allocation hierarchy
- frame derivation/retyping
- multiple mappings
- cache/device attributes
- pinning and DMA ownership
- memory accounting or quotas
- user-space memory server protocol

Action: implement boot-time physical memory discovery and capability-based delegation to a userspace memory server.

### High: Capability model lacks production derivation and revocation semantics

Strengths include rights masks, generation references, copy/move/delete, and bounded CSpaces. Gaps include:

- no capability derivation tree
- `revoke_reference()` is effectively empty
- revocation requires external iteration over known CSpaces
- no badge/guard/depth semantics
- fixed flat CSpace with 32 slots
- no atomic multi-cap transfer
- no ownership/accounting model
- no robust authority proof for object destruction across all descendants

Action: define a derivation/revocation model and scalable CSpace lookup before stabilizing ABI v1.

### High: IPC is not yet production L4 IPC

The endpoint object has bounded sender and receiver queues, but the system remains bring-up oriented. Missing or incomplete areas include:

- fastpath/slowpath separation
- priority inheritance or scheduling-context donation
- timeout model
- cancellation races and endpoint destruction proof
- capability transfer in messages
- reply capabilities or protected reply objects
- fault IPC protocol
- large-message transfer strategy
- formal atomicity rules for call/reply-receive
- cross-CPU targeted reschedule; current helper broadcasts to all other CPUs

Action: specify and implement the complete IPC state machine before adding more hypervisor profiles.

### High: Scheduler is not aligned with the RT goal

The generic scheduler is a simple fixed-size per-CPU round-robin queue. Scheduling contexts contain priority, budget, and period fields, but the scheduler does not use priority or enforce periodic replenishment. Missing:

- priority queues
- budget enforcement
- replenishment queues
- deadline/period semantics
- priority inheritance/donation
- bounded preemption latency measurements
- migration locking and load balancing
- CPU hotplug/offline handling
- RT-safe lock hierarchy

Action: make the scheduling-context object operational and define a verified RT scheduling policy.

### High: Interrupt objects and userspace drivers are incomplete

The interrupt object stores an IRQ number and notification reference, but acknowledgement simply reads and completes an interrupt. There is no complete capability-authorized bind/mask/unmask lifecycle, trigger configuration, ownership transfer, storm handling, or teardown synchronization. The UART remains effectively a kernel console rather than a production userspace driver path.

Action: implement IRQ control objects and transition normal console ownership to a userspace serial driver, retaining only an emergency kernel debug sink.

### High: AMD64 is build compatibility only

AMD64 exception handling, SMP startup, APIC, address-space mapping, interrupts, and hypervisor support contain placeholders or return `unsupported`. The README’s “ARM64 and AMD64” description can imply more parity than exists.

Action: clearly label AMD64 as compile-only until it boots and passes the kernel profile.

### High: Test files are mostly placeholders

Many `.tt` files contain only `static_assert(true)`. Runtime self-tests are useful but heavily embedded in production headers. Missing:

- host unit tests for pure logic
- property-based capability/IPC tests
- race tests under controlled scheduling
- sanitizer builds for host-testable components
- coverage reports
- model checking for lifecycle state machines
- long soak and fault-injection automation
- CI matrices and reproducible certification records

Action: establish a layered test strategy and move test implementation out of production headers.

### Medium: Header-oriented structure has become monolithic

`kernel/hypervisor.hh` is over 1,250 lines and mixes:

- object definitions
- VMID allocation
- stage-2 management
- vCPU execution
- guest test payload orchestration
- diagnostics
- Profiles 0.1–0.6 acceptance models

This increases compile coupling and makes privilege-boundary review difficult.

Action: split into object, stage2, vcpu, interrupt, lifecycle, diagnostics, and test-only modules.

### Medium: ABI is not truly C-compatible yet

The repository describes `include/abi` as C-compatible, but ABI headers are `.hh`, use namespaces, scoped enums, and C++ types. Several ABI domain headers are empty placeholders. There is also inconsistency between object types in the ABI and kernel, such as the kernel having a task object while the ABI object list omits it.

Action: either define a real C ABI in `.h` files or explicitly declare ABI v1 C++-only. Do not freeze the current interface.

### Medium: Boot and system image path is incomplete

The boot information includes earlyfs fields, but the current root image path embeds or directly loads the test root executable. Missing:

- robust earlyfs format and validation
- ELF loader in userspace or minimal kernel bootstrap loader
- dynamic linker and libc plan execution
- real-root discovery and mount handoff
- service manifest and dependency graph
- signed image and measured-boot policy

### Medium: Logging is not fully RT-safe or production-observable

The custom formatter avoids `va_list`, which is useful. Remaining issues:

- recursive same-CPU logging is unsupported
- likely synchronous serial output in privileged paths
- no per-CPU lockless ring buffers
- no structured event IDs/stable telemetry ABI
- verbose MMU diagnostics are mixed into normal boot
- no userspace log drain or crash record persistence

Action: implement per-CPU bounded rings and defer device output to a userspace logging service.

## Goal-alignment matrix

| Goal | Current state | Assessment |
|---|---|---|
| Small control-plane kernel | Kernel mechanisms are bounded, but test and hypervisor models are embedded in kernel | Partial |
| Policy in userspace | Root test runs in userspace; actual servers absent | Major gap |
| Capability security | Basic rights/generations work; derivation/revocation incomplete | Partial |
| L4 IPC | Basic syscall and endpoint queues work | Early |
| MMU/interrupts/SMP | Real ARM64 implementation works | Strong |
| RT scheduling | Fields exist; policy not implemented | Major gap |
| Userspace drivers | Mostly placeholders | Major gap |
| Domain-management OS | Hypervisor foundations work; domain manager absent | Early |
| Multiple guest domains | Model tests only beyond single real guest | Major gap |
| Production lifecycle | Bounded tests are good; global production model incomplete | Partial |
| AMD64 support | Compile-only placeholders | Not implemented |
| Stable native ABI | Test operations and placeholders pollute v1 | Not ready |
| Production verification | Runtime acceptance is useful; unit/formal/CI layers absent | Early |

## Recommended reset of the roadmap

Do not continue directly to Hypervisor Profiles 0.7–1.0. The highest-value next milestone is a cross-cutting architecture-alignment release.

### Alignment Milestone A: Separate product from certification harness

1. Add `CONFIG_SELFTEST` and `CONFIG_HYPERVISOR_SELFTEST`.
2. Move profile code out of `kernel/hypervisor.hh` into test-only sources.
3. Remove acceptance/fuzz operations from the production syscall ABI.
4. Split hypervisor implementation into reviewed modules.
5. Rename model-only results so they cannot be mistaken for real guest execution.

### Alignment Milestone B: Real userspace control plane

Implement and boot:

1. root resource server
2. memory server and pager
3. process/ELF loader
4. device/IRQ resource server
5. console server with userspace UART driver
6. domain manager/VMM
7. supervision service

The kernel should only bootstrap the root task and delegate resources.

### Alignment Milestone C: Complete L4 primitives

1. capability derivation and revoke
2. scalable CSpaces
3. complete IPC call/receive/reply-receive state machine
4. reply capability semantics
5. scheduling-context donation
6. fault IPC
7. timeout and cancellation semantics

### Alignment Milestone D: RT kernel foundation

1. priority scheduler
2. budget and period enforcement
3. replenishment
4. priority inheritance/donation
5. bounded locks and lock-order checks
6. per-CPU RT-safe logging
7. latency instrumentation and acceptance thresholds

### Alignment Milestone E: Real Hypervisor 0.7

Only after A–D foundations are stable:

1. secondary physical CPUs independently call `enter_guest`
2. four real guest vCPUs execute guest instructions
3. guest-side SMP rendezvous and shared-memory barriers
4. real cross-vCPU virtual IPI delivery
5. independent virtual timer state
6. preempt/save/migrate/re-enter on another physical CPU
7. two real guests executing concurrently
8. teardown and fault containment under real execution

## Immediate recommendation

Treat patch 0061 as a useful tested control-plane-model baseline, not as a production Hypervisor 0.6 milestone. Freeze feature profiles temporarily and create an `architecture_alignment.md` plus a revised implementation roadmap. The next code batch should perform Alignment Milestone A, then start the userspace memory/process/server path.
