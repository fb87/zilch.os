# Certification guest blob adapter

`guest_blob.S` is a certification-only packaging adapter. It embeds the raw
binary produced from `src/user/guests/test-arm64/guest-test.elf` and exposes
start/end symbols to the temporary in-kernel real-single-vCPU verification
harness. Release builds do not compile this adapter.

The adapter is transitional and must be removed when the PL3 domain manager
loads guest ELF files from earlyfs into delegated VM memory.
