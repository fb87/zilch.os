---
module: sys.user.bootstrap.embedded_images_amd64
---

amd64's counterpart to `embedded_images.S`, covering only that file's
`CONFIG_ROOT_ONLY_BOOT` branch: an `.incbin` wrapper with start/end
markers, architecture-neutral GNU-as directives with no actual
instructions. Only the symbol prefix needed to change (`sys_amd64_` in
place of `sys_arm64_`) to match what `src/arch/amd64/include/sys/arch/
space/address_space.hh` already declares and extern "C"-references.

Does not cover `embedded_images.S`'s `#else` branch: that is a
hand-written AArch64 assembly stress-test program (a server/client IPC
fuzzing loop), arm64-specific in its instructions rather than just its
symbol names. `src/image/image.mk` only builds this translation unit
under `CONFIG_ROOT_ONLY_BOOT`, this project's default boot profile
(`BOOT_PROFILE=root`), so the non-root-only branch was not needed to
unblock amd64's build.

Same ownership rationale as `embedded_images.S`: this is packaging policy,
not kernel logic, so it lives under `src/user/` even though it produces an
object the kernel image links against.
