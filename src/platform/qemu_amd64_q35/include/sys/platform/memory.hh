#pragma once

#include <sys/platform/v1/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x00100000ULL;
    inline constexpr psize_t ram_size = 255ULL * 1024ULL * 1024ULL;
    [[nodiscard]] inline constexpr bool valid_device_page(paddr_t) noexcept {
        return false;
    }
} // namespace sys::platform::memory
