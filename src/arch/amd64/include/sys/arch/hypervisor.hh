#pragma once

#include <sys/types.hh>

namespace sys::arch::hypervisor
{
    inline constexpr bool available = true;
    inline constexpr bool active = false;

    [[nodiscard]] inline constexpr u64 sanitize_guest_pstate(u64 value) noexcept {
        return value;
    }
    [[nodiscard]] inline constexpr u64 sanitize_guest_sctlr_el1(u64 value) noexcept {
        return value;
    }
    [[nodiscard]] inline constexpr u64 sanitize_guest_tcr_el1(u64 value) noexcept {
        return value;
    }
    [[nodiscard]] inline constexpr u64 sanitize_guest_cpacr_el1(u64 value) noexcept {
        return value;
    }
    [[nodiscard]] inline constexpr u64 sanitize_guest_cntkctl_el1(u64 value) noexcept {
        return value;
    }
    [[nodiscard]] inline bool virtual_gic_hardware_available() noexcept {
        return false;
    }
    [[nodiscard]] inline bool consume_virtual_irq_acknowledgement() noexcept {
        return false;
    }

    [[nodiscard]] inline error_t initialize() noexcept {
        return error_t::unsupported;
    }

    [[nodiscard]] inline bool initialize_cpu() noexcept {
        return true;
    }

    [[nodiscard]] inline u32 verified_count() noexcept {
        return 0U;
    }

    [[nodiscard]] inline error_t configure_host() noexcept {
        return error_t::unsupported;
    }
    inline void invalidate_stage2(u16) noexcept {}
    template <typename Context, typename Exit>
    [[nodiscard]] inline error_t run_guest(u16, paddr_t, Context&, Exit&, u64,
                                           bool = false) noexcept {
        return error_t::unsupported;
    }

} // namespace sys::arch::hypervisor
