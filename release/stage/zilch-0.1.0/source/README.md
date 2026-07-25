# Zilch L4 Microkernel Skeleton

A dual-architecture, dual-platform, restricted C++20 L4-style microkernel and
userspace skeleton.

Supported pairs:

- `ARCH=arm64 PLATFORM=qemu-arm64-virt`
- `ARCH=amd64 PLATFORM=qemu-amd64-q35`

Build a complete development bundle for one target:

```sh
make arm64
make amd64
```

Each build produces:

```text
build/<arch>/<platform>/
├── zilch.elf
├── zilch.bin
├── zilch.map
├── user/init.elf
├── user/init.map
└── image/earlyfs.tar
```

Run either kernel with the unified runner:

```sh
./scripts/run.sh build/arm64/qemu-arm64-virt/zilch.elf
./scripts/run.sh build/amd64/qemu-amd64-q35/zilch.elf
```

The current kernel boot path reaches its diagnostic marker. Userspace is built
and packaged now, but kernel-side ELF loading, root-task address-space creation,
capability bootstrap, and PL3 entry remain planned milestones.

Create a dual-target source/system release:

```sh
make release VERSION=0.1.0
```
