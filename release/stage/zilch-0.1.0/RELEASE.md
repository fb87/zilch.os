# zilch 0.1.0

This release contains a clean source export and two system-development bundles:

- ARM64 / QEMU virt
- AMD64 / QEMU q35

Each bundle includes the kernel ELF/raw image/map, the initial userspace
`init.elf`, and an earlyfs archive. The kernel skeleton currently boots to its
diagnostic marker; loading and entering the root task is the next boot-flow
milestone.

Run a kernel explicitly:

`./run.sh systems/arm64-qemu-virt/kernel.elf`
`./run.sh systems/amd64-qemu-q35/kernel.elf`
