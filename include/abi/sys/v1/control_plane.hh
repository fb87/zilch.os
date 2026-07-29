#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class control_plane_role : word_t {
        process = 0x200U,
        device = 0x201U,
        console = 0x202U,
        domain = 0x203U,
        supervisor = 0x204U,
    };

    enum class control_plane_operation : word_t {
        health = 0U,
        describe = 1U,
        stop = 2U,
    };

    inline constexpr word_t control_plane_role_count = 5U;
    inline constexpr word_t memory_service_ready_badge = 1U << 5U;
    inline constexpr word_t control_plane_health_magic = 0x4845414cU;
    inline constexpr word_t control_plane_exit_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (8U + role - first)
                   : 0U;
    }
    inline constexpr word_t control_plane_ready_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (role - first)
                   : 0U;
    }
} // namespace sys::abi::v1
