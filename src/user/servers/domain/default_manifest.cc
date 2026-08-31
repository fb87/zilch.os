/* SPDX-License-Identifier: Apache-2.0 */
#include <sys/guest_manifest.hh>

/*
 * Fallback manifest used when no guest package supplies its own via
 * DOMAIN_GUEST_MANIFEST. The built-in test guest (src/user/guests/test-arm64,
 * entry.S) never touches the UART, so it has no device passthrough needs;
 * the physical PL011 is exclusively owned by the console-server, same as
 * every other guest -- see samples/guests/zephyr/manifest.cc for the guest
 * that actually drives its UART, via vPL011 trap-and-emulate rather than
 * direct passthrough.
 */
extern "C" const sys::guest_manifest::manifest sys_arm64_domain_guest_manifest = {
    .ram_size = 32U * 4096U,
    .guest_stack = 0xf000U,
    .guest_pstate = 0x3c5U,
    .device_count = 0U,
    .devices = {},
};
