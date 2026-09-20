# ext2 on-disk format

Read-only structure definitions for an EXTERNAL format. Unlike `abi/v1`,
which this system defines and freezes itself, these layouts are fixed by the
ext2 specification and must match byte-for-byte whatever wrote the image.

Every struct is packed and offset-checked with `static_assert`, the same
discipline `sys::vmm::elf` applies to guest ELF headers and for the same
reason: this is untrusted external input. A field at the wrong offset is a
parsing bug that reads attacker-chosen bytes, so the layout is asserted at
compile time rather than trusted.
