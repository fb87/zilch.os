/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Certification-only adapter. The executable payload is owned and built as
 * src/user/guests/test-arm64/guest-test.elf; this object only exposes its raw
 * image to the temporary in-kernel real-guest verification harness.
 */
.section .rodata.guest_image,"a"
.balign 4096
.global sys_arm64_guest_image_start
.global sys_arm64_guest_image_end
sys_arm64_guest_image_start:
.incbin GUEST_TEST_BIN_PATH
sys_arm64_guest_image_end:
