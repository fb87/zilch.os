#pragma once

#include <sys/platform/v1/types.hh>
#include <sys/types.hh>

namespace sys::platform::firmware
{
    inline constexpr v1::boot_info_t boot_info{
        .firmware_data = 0U,
        .earlyfs_base = 0U,
        .earlyfs_size = 0U,
        .boot_cpu = 0U,
        .cpu_count = 1U,
    };

    [[nodiscard]] inline error_t start_cpu(cpu_id_t, uintptr_t, uintptr_t) noexcept
    {
        return error_t::unsupported;
    }
} // namespace sys::platform::firmware
