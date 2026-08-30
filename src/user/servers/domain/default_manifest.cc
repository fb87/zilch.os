/* SPDX-License-Identifier: Apache-2.0 */
#include <sys/guest_manifest.hh>

/*
 * Fallback manifest used when no guest package supplies its own via
 * DOMAIN_GUEST_MANIFEST. Matches the qemu_arm64_virt platform's PL011 UART,
 * which the built-in test guest (src/user/guests/test-arm64) also expects
 * mapped at this address.
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
