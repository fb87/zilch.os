---
module: sys.user.bootstrap.embedded_images
---

`embedded_images.S` is the temporary root-bootstrap packaging adapter. It embeds
independently linked PL3 ELF files as read-only kernel image data so the minimal
kernel bootstrap can create the initial root task before a userspace process
server and pathname-based earlyfs loader exist.

Ownership is deliberately under `src/user/`: the payloads and packaging policy
are userspace concerns. The kernel consumes only exported image boundaries.
This adapter must disappear when the root process server loads `/bin/init`,
`/bin/domain-manager`, and other services from delegated earlyfs resources.
