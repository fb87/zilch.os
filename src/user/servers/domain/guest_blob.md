---
module: sys.user.servers.domain.guest_blob
---

`guest_blob.S` is a certification-only adapter that embeds a small ZEFS1
earlyfs image (see `include/sys/platform/v1/earlyfs.hh`), built from the
externally selected `DOMAIN_GUEST_ELF` by `tools/image/make_earlyfs.py`, as
read-only data in the domain-manager image. `main.cc` looks the guest ELF up
by name (`guest.elf`) via the same `earlyfs::find()` reader used for the
kernel's own PL3-server image, rather than referencing raw incbin symbols
directly. It allows the PL3 loader and lifecycle protocol to be exercised
before the domain-manager has its own capability-delegated access to the
kernel's earlyfs image (see the project roadmap's path-based process-creation
item).

Release builds exclude this adapter. Guest samples own the production of their
ELF artifacts and pass only the resulting path across the generic build
boundary.
