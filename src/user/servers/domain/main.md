# Domain-manager service executable

The independently linked PL3 domain service owns VM construction and guest
policy for control-plane role `0x203`. Its private endpoint supports health,
description, create, load, single-exit run, interactive serve, destroy, and
service-stop operations.

The current bounded loader parses ARM64 ELF64 images, copies loadable content
through temporary frame mappings, derives final stage-2 permissions from
allocated ELF sections, zeroes remaining guest RAM, rejects W+X pages, and
configures the vCPU at the ELF entry point. Device policy currently delegates a
single PL011 page for the Zephyr sample. The generic service does not fetch or
build guest operating systems.

Interactive serve mode repeatedly re-enters resumable wait/timer exits. It is a
sample and bring-up mechanism selected at build time, not the release management
policy.
