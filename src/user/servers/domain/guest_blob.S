/* SPDX-License-Identifier: Apache-2.0 */
.section .rodata.domain_guest,"a"
.balign 4096
.global sys_arm64_guest_earlyfs_image_start
.global sys_arm64_guest_earlyfs_image_end
sys_arm64_guest_earlyfs_image_start:
.incbin GUEST_EARLYFS_PATH
sys_arm64_guest_earlyfs_image_end:
