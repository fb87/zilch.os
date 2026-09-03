#if CONFIG_ROOT_ONLY_BOOT
/*
 * amd64's counterpart to embedded_images.S's CONFIG_ROOT_ONLY_BOOT branch.
 * Just an incbin wrapper with start/end markers -- architecture-neutral
 * GNU-as directives, no actual instructions -- so this only needed the
 * sys_arm64_ symbol prefix changed to sys_amd64_ to match what
 * src/arch/amd64/include/sys/arch/space/address_space.hh already declares
 * and extern "C"-references.
 *
 * Does not have a counterpart to that file's #else branch: that is a
 * hand-written AArch64 assembly stress-test program (server/client IPC
 * fuzzing loop), arm64-specific in its instructions, not just its symbol
 * names, and out of scope here -- image.mk only builds this translation
 * unit under CONFIG_ROOT_ONLY_BOOT, which is this project's default boot
 * profile (BOOT_PROFILE=root), so the non-root-only branch was not
 * needed to unblock amd64's build.
 */
.section .user_text,"ax"
.balign 4096
.global sys_amd64_earlyfs_image_start
sys_amd64_earlyfs_image_start:
.incbin EARLYFS_IMAGE_PATH
.balign 4096
.global sys_amd64_earlyfs_image_end
sys_amd64_earlyfs_image_end:
#endif
