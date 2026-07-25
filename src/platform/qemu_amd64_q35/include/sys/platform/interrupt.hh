#pragma once

#include <sys/types.hh>

namespace sys::platform::interrupt
{
    inline constexpr irq_id_t reschedule_ipi = 0U;
    inline constexpr irq_id_t tlb_shootdown_ipi = 1U;
    inline constexpr irq_id_t virtual_timer_irq = 0U;
    inline void initialize_global() noexcept {}
    [[nodiscard]] inline error_t initialize_cpu() noexcept { return error_t::unsupported; }
    [[nodiscard]] inline error_t initialize() noexcept { return error_t::unsupported; }
    [[nodiscard]] inline irq_id_t acknowledge() noexcept { return 0U; }
    inline void complete(irq_id_t) noexcept {}
    inline void send_ipi_all_others(irq_id_t) noexcept {}
} // namespace sys::platform::interrupt
