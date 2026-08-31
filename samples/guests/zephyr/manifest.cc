/* SPDX-License-Identifier: Apache-2.0 */
#include <sys/guest_manifest.hh>

/*
 * What this guest needs from the domain-manager: 128 KiB of RAM (matching
 * hypervisor.overlay's sram0 size) and the boot register state this guest's
 * EL2 bootstrap protocol expects. No passthrough devices: the PL011 UART at
 * 0x09000000 is deliberately left unmapped so guest accesses trap as MMIO
 * exits and are emulated by the domain-manager's vPL011 module, which
 * forwards real character I/O through the console-server -- see
 * src/user/servers/domain/main.cc. The real UART hardware is exclusively
 * owned by the console-server (root_graph.hh mints its device frame), not
 * this guest; direct passthrough would collide with that ownership.
 */
extern "C" const sys::guest_manifest::manifest sys_arm64_domain_guest_manifest = {
    .ram_size = 32U * 4096U,
    .guest_stack = 0xf000U,
    .guest_pstate = 0x3c5U,
    .device_count = 0U,
    .devices = {},
};
