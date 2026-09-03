#pragma once

#include <sys/platform/v1/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x00100000ULL;
    inline constexpr psize_t ram_size = 255ULL * 1024ULL * 1024ULL;

    /* amd64 has no FDT-based boot protocol, so there are no fallback
     * addresses to retry an FDT parse against (see arm64's counterpart).
     */
    inline constexpr const paddr_t* boot_inventory_probes = nullptr;
    inline constexpr u32 boot_inventory_probe_count = 0U;

    [[nodiscard]] inline constexpr bool valid_device_page(paddr_t) noexcept {
        return false;
    }
} // namespace sys::platform::memory
