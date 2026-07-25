#pragma once

#include <sys/types.hh>

namespace sys::arch::v1
{
    struct version_t {
        u16 major;
        u16 minor;
        u16 patch;
    };

    inline constexpr version_t required_version{
        .major = 1U,
        .minor = 0U,
        .patch = 0U,
    };

    [[nodiscard]]
    constexpr bool compatible(version_t provided) noexcept {
        return provided.major == required_version.major &&
               provided.minor >= required_version.minor;
    }
} // namespace sys::arch::v1
