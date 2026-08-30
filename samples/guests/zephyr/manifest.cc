/* SPDX-License-Identifier: Apache-2.0 */
#include <sys/guest_manifest.hh>

/*
 * What this guest needs from the domain-manager: 128 KiB of RAM (matching
 * hypervisor.overlay's sram0 size), a PL011 UART passed through at its real
 * physical address with SPI 33 forwarded, and the boot register state this
 * guest's EL2 bootstrap protocol expects.
 */
extern "C" const sys::guest_manifest::manifest sys_arm64_domain_guest_manifest = {
    .ram_size = 32U * 4096U,
    .guest_stack = 0xf000U,
    .guest_pstate = 0x3c5U,
    .device_count = 1U,
    .devices = {{.ipa = 0x09000000U,
                .size = 4096U,
                .permissions = 1U | 2U | (1U << 3U),
                .forward_irq = 33U,
                .forward_trigger = 0U}},
};
