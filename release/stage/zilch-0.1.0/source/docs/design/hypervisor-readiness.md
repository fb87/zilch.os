# Hypervisor Readiness

The v1 architecture contract reserves virtualization operations for stage-2/EPT address spaces, guest execution, virtual interrupt injection, and VM exit decoding. The initial boot skeleton does not enable VMX or ARM virtualization extensions. Production implementation must keep guest-domain policy in user space and retain only privileged virtualization mechanisms in the kernel.
