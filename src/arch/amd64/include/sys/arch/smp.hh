#pragma once

#include <sys/types.hh>

namespace sys::arch::smp
{
    inline void mark_online() noexcept {}
    [[nodiscard]] inline u32 online_count() noexcept { return 1U; }
    [[nodiscard]] inline error_t boot_secondary_cpus() noexcept { return error_t::unsupported; }
    inline bool wait_until_online(u32 expected, u64) noexcept { return expected <= 1U; }
    inline void record_reschedule_ipi() noexcept {}
    inline void record_tlb_shootdown_ipi() noexcept {}
    [[nodiscard]] inline u32 reschedule_ipis(cpu_id_t) noexcept { return 0U; }
    [[nodiscard]] inline u32 tlb_shootdown_ipis(cpu_id_t) noexcept { return 0U; }
} // namespace sys::arch::smp
