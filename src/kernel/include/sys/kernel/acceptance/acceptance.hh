#pragma once

#include <sys/kernel/printk.hh>
#include <sys/types.hh>

namespace sys::kernel::acceptance
{
#if CONFIG_SELFTEST
    inline constexpr u64 certification_operation_target = 1048576U;

    inline volatile bool authority_ready{};
    inline volatile bool memory_ready{};
    inline volatile bool notification_ready{};
    inline volatile bool fault_ipc_observed{};
    inline volatile bool final_reported{};

    inline void report(const char* name, bool passed) noexcept {
        pr_info("[TEST] name=%s result=%s\n", name, passed ? "PASS" : "FAIL");
    }

    inline void mark_profile_self_tests(bool authority, bool memory, bool notification) noexcept {
        __atomic_store_n(&authority_ready, authority, __ATOMIC_RELEASE);
        __atomic_store_n(&memory_ready, memory, __ATOMIC_RELEASE);
        __atomic_store_n(&notification_ready, notification, __ATOMIC_RELEASE);
        report("capability_lifecycle", authority);
        report("memory_map_unmap_wx", memory);
        report("notification_signal_consume", notification);
    }

    inline void mark_fault_ipc() noexcept {
        const bool previous = __atomic_exchange_n(&fault_ipc_observed, true, __ATOMIC_ACQ_REL);
        if (!previous)
            report("fault_ipc_delivery", true);
    }

    [[nodiscard]] inline bool prerequisites_ready() noexcept {
        return __atomic_load_n(&authority_ready, __ATOMIC_ACQUIRE) &&
               __atomic_load_n(&memory_ready, __ATOMIC_ACQUIRE) &&
               __atomic_load_n(&notification_ready, __ATOMIC_ACQUIRE) &&
               __atomic_load_n(&fault_ipc_observed, __ATOMIC_ACQUIRE);
    }

    inline void report_final(u64 operations, u64 failures, bool all_cpus_progressed) noexcept {
        if (operations < certification_operation_target || failures != 0U || !all_cpus_progressed ||
            !prerequisites_ready())
            return;
        bool expected = false;
        if (!__atomic_compare_exchange_n(&final_reported, &expected, true, false, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            return;
        report("smp_all_cpus_progress", true);
        report("deterministic_fuzz", true);
        pr_info("[ACCEPTANCE] profile=1.0 phase=runtime result=PASS operations=%llu failures=0\n",
                static_cast<unsigned long long>(operations));
        pr_info("[ACCEPTANCE] profile=1.0 result=PASS failures=0\n");
    }
#else
    inline void mark_profile_self_tests(bool, bool, bool) noexcept {}
    inline void mark_fault_ipc() noexcept {}
    inline void report_final(u64, u64, bool) noexcept {}
#endif
} // namespace sys::kernel::acceptance
