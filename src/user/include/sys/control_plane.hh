#pragma once

#include <sys/types.hh>

#include <abi/sys/v1/control_plane.hh>

namespace sys::control_plane
{
    struct service_policy final {
        abi::v1::control_plane_role role{};
        word_t dependency_mask{};
        word_t restart_limit{};
        word_t quota_pages{};
        bool privileged{};
    };

    [[nodiscard]] inline constexpr service_policy policy_for(word_t raw_role) noexcept {
        using role = abi::v1::control_plane_role;
        switch (static_cast<role>(raw_role)) {
            case role::process:
                return {role::process, 0U, 3U, 32U, true};
            case role::device:
                return {role::device, 1U << 0U, 3U, 16U, true};
            case role::console:
                return {role::console, (1U << 0U) | (1U << 1U), 5U, 8U, false};
            case role::domain:
                return {role::domain, 1U << 0U, 2U, 32U, true};
            case role::supervisor:
                return {role::supervisor, 0x0fU, 0U, 8U, false};
        }
        return {};
    }

    [[nodiscard]] inline constexpr bool valid(const service_policy& policy) noexcept {
        const word_t role = static_cast<word_t>(policy.role);
        const word_t first = static_cast<word_t>(abi::v1::control_plane_role::process);
        return role >= first && role < first + abi::v1::control_plane_role_count &&
               policy.quota_pages != 0U && (policy.dependency_mask & ~0x0fU) == 0U;
    }

    [[nodiscard]] inline constexpr bool may_restart(const service_policy& policy,
                                                    word_t attempts) noexcept {
        return valid(policy) && attempts < policy.restart_limit;
    }
} // namespace sys::control_plane
