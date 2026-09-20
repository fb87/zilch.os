# Retired profiles

Historical records of profiles that have been superseded. They are kept as
written, not edited to match what is true now — an archive that gets
tidied up to agree with the present stops being evidence of anything.

**None of these is a current claim.** For what is actually implemented and
verified today, read `docs/readiness/PRODUCTION_READINESS_CHECKLIST.md`,
which is authoritative, and `docs/readiness/HYPERVISOR_GUEST_ARCHITECTURE.md`
for the hypervisor specifically.

## How to read the hypervisor profiles

Profiles 0.1 through 0.6 were written as the hypervisor was being built, and
their scope grew across the sequence. Two things about them mislead if read
casually:

**Bounded means bounded.** Each profile states its own limits up front and
means them literally. Profile 0.1's "bounded virtual platform" is one VM,
one vCPU, 64 KiB of guest RAM and a guest IPA range of
`0x00000000-0x0000ffff`, with no virtual GIC, virtio, passthrough or SMP
guest. A `result=PASS` in that document is a pass against those limits and
nothing wider.

**Model results and real execution are both present, and the distinction
matters more than the formatting suggests.** Results named with a `-model`
suffix — `guest-smp-model`, the "bounded concurrent-execution model" in 0.4
— are bounded-model results: the control plane and its state machine were
exercised, not guest instructions on a physical CPU. Results without it, in
the profiles that had reached real execution, are real. Current work uses
`HV-MODEL` and `hypervisor_control_model` to make this unmissable, but
these documents predate that convention and are not being retrofitted with
it.

If a specific historical result matters, the reliable move is to check
whether the same property appears in the current checklist with its own
evidence, rather than to carry the profile's claim forward.

## Kernel and acceptance profiles

`KERNEL_PROFILE_1_0*`, `PROFILE_1_0_ACCEPTANCE` and
`PROFILE_1_0_RUNTIME_CERTIFICATION` record the kernel 1.0 sequence on the
same terms. The same caution applies: they describe the state at the time
they were written, and the checklist is what describes the state now.
