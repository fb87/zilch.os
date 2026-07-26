#pragma once

#include <sys/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x40000000ULL;
    inline constexpr psize_t ram_size =
        static_cast<psize_t>(CONFIG_QEMU_RAM_MB) * 1024ULL * 1024ULL;
    static_assert(CONFIG_QEMU_RAM_MB >= 128);
} // namespace sys::platform::memory
