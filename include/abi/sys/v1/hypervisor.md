# Hypervisor ABI v1

VM and vCPU authority is carried by capabilities. Stage-2 mapping takes a guest
IPA, a frame capability selector, and validated permissions; userspace cannot
submit a physical address. The kernel requires normal/device frame type to match
the requested mapping type.

`vcpu_run` returns six machine words:

1. syscall status;
2. exit reason;
3. syndrome;
4. fault address;
5. guest program counter;
6. qualification.

For stage-2 faults, qualification is the reconstructed IPA. For rejected guest
hypercalls, it is the original call number. WFI/WFE exits preserve ESR in the
syndrome and advance the guest PC before returning `wait`.

Lifecycle operations provide configure, run, pause, resume, stop, reset, map,
unmap, and bounded dynamic VM/vCPU create and destroy. VM destruction is busy
while a child vCPU, active execution, or stage-2 mapping remains. Destruction
revokes every capability before unregistering and scrubbing the reusable slot.
