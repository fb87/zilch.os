---
module: sys.user.servers.domain.guest_blob
---

`guest_blob.S` is a certification-only adapter that embeds the externally
selected `DOMAIN_GUEST_ELF` as read-only data in the domain-manager image. It
allows the PL3 loader and lifecycle protocol to be exercised before pathname
lookup and external image transport are complete.

Release builds exclude this adapter. Guest samples own the production of their
ELF artifacts and pass only the resulting path across the generic build
boundary.
