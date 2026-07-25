#pragma once

#include <sys/platform/v1/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x40000000ULL;
    inline constexpr psize_t ram_size = 256ULL * 1024ULL * 1024ULL;
} // namespace sys::platform::memory
