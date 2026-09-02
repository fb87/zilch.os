/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Minimal ARM64 guest that exercises the *production* guest-hosting path:
 * stage-2 trap-and-emulate through the domain manager's vPL011.
 *
 * Deliberately not the same shape as src/user/guests/test-arm64, which
 * speaks a guest-visible HVC contract to the certification harness. That
 * guest cannot run under the domain manager, which hosts guests by trapping
 * MMIO rather than by servicing hypercalls -- which is why it is gated on
 * CONFIG_SELFTEST. This one touches no hypercall at all: every store below
 * is an ordinary device write that faults to stage-2 and is emulated by
 * sys::vmm::vpl011, forwarded to console-server, and printed on the real
 * UART by the serial driver.
 *
 * Loaded flat at guest-physical 0 with entry 0 (the domain manager's
 * loader takes the non-ELF path for a raw image, and the ELF path lands
 * here too since this links at 0).
 */

.equ VPL011_BASE, 0x09000000     /* matches sys::vmm::vpl011::base_ipa */
.equ VPL011_DR,   0x00           /* data register: byte out              */

.section .text.guest_image,"ax"
.balign 4096
.global sys_arm64_vpl011_guest_start
.global sys_arm64_vpl011_guest_end

sys_arm64_vpl011_guest_start:
    /*
     * x0 holds the vPL011 base for the whole routine. Each store is one
     * trapped access -- the model buffers them and flushes a line at a
     * time, so the cost of a trap per byte is paid once per character but
     * the IPC cost is not.
     */
    movz    x0, #0x0900, lsl #16
    adr     x1, message

1:  ldrb    w2, [x1], #1
    cbz     w2, 2f
    str     w2, [x0, #VPL011_DR]
    b       1b

    /*
     * Park forever. WFE rather than a tight branch so the vCPU yields
     * instead of spinning the host: the domain manager treats a WFx exit as
     * idle and stops re-entering, which is also what lets its console
     * flush actually reach the serial driver.
     */
2:  wfe
    b       2b

message:
    .asciz  "guest alive via vpl011\n"

.balign 4
sys_arm64_vpl011_guest_end:
