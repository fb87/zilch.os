# Verification Plan

CI shall build both target pairs, scan forbidden symbols and sections, boot each image in QEMU, assert the UART marker, execute constexpr contract tests, and archive ELF/map/disassembly artifacts. Later phases add unit tests, property tests, fault injection, WCET evidence, coverage, SMP stress, and formal checks of capability and IPC invariants.
