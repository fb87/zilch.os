# ARM64 verification guest entry

`entry.S` implements the independently linked ARM64 certification guest. It
boots at guest physical address zero, establishes EL1 stage-1 translation,
installs exception vectors, validates virtual timer and EL0 transitions, and
reports results through the guest-visible HVC contract.

The source is owned by userspace guest software and may not include private
kernel headers or reference kernel namespaces. Certification packages the ELF
as `/guests/test-arm64.elf`; release images omit it.
