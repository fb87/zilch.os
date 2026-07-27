#pragma once

#include <sys/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x40000000ULL;
    inline constexpr psize_t ram_size =
        static_cast<psize_t>(CONFIG_QEMU_RAM_MB) * 1024ULL * 1024ULL;
    inline constexpr paddr_t uart_base = 0x09000000ULL;
    inline constexpr psize_t uart_size = 0x1000ULL;

    [[nodiscard]] inline constexpr bool valid_device_page(paddr_t address) noexcept {
        return address == uart_base;
    }

    static_assert(CONFIG_QEMU_RAM_MB >= 128);
} // namespace sys::platform::memory
