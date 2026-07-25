#pragma once

#include <sys/platform/v1/contract.hh>
#include <sys/platform/console.hh>
#include <sys/platform/firmware.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/memory.hh>
#include <sys/platform/timer.hh>

namespace sys::platform
{
    inline constexpr const char* name = "qemu_arm64_virt";
    inline constexpr v1::version_t version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    static_assert(v1::compatible(version));
} // namespace sys::platform
