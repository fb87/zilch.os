#pragma once

#include <sys/platform/firmware.hh>
#include <sys/types.hh>

namespace sys::arch::smp
{
    inline void mark_online() noexcept {}
    [[nodiscard]] inline u32 online_count() noexcept {
        return 1U;
    }

    /*
     * Mirrors arm64's contract (src/arch/arm64/include/sys/arch/smp.hh):
     * "no secondary CPUs to boot" is success, not failure -- arm64's own
     * implementation returns success trivially for a 1-CPU count, since
     * its loop from cpu_id=1 to count-1 does zero iterations. This used to
     * return unsupported unconditionally, which kernel.hh's start()
     * treats as fatal regardless of how many CPUs were actually expected
     * -- with this platform's boot_info.cpu_count hardcoded to 1 (see
     * platform/firmware.hh), that meant amd64 halted immediately after
     * printing "smp: boot CPU online" on every boot, before reaching
     * anything after it.
     *
     * amd64 genuinely has no INIT-SIPI-SIPI secondary-CPU bring-up
     * implemented yet (Phase 11 territory), so if cpu_count ever exceeds 1
     * on this platform, this honestly reports unsupported rather than
     * silently claiming success for work it cannot do.
     */
    [[nodiscard]] inline error_t boot_secondary_cpus() noexcept {
        return platform::firmware::boot_info.cpu_count <= 1U ? error_t::success
                                                             : error_t::unsupported;
    }
    inline bool wait_until_online(u32 expected, u64) noexcept {
        return expected <= 1U;
    }
    inline void record_reschedule_ipi() noexcept {}
    inline void record_tlb_shootdown_ipi() noexcept {}
    [[nodiscard]] inline u32 reschedule_ipis(cpu_id_t) noexcept {
        return 0U;
    }
    [[nodiscard]] inline u32 tlb_shootdown_ipis(cpu_id_t) noexcept {
        return 0U;
    }
} // namespace sys::arch::smp
