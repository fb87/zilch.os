#pragma once

#include <sys/kernel/printk.hh>
#include <sys/types.hh>

namespace sys::kernel::verification
{
#if CONFIG_SELFTEST
    inline constexpr u64 certification_operation_target = 1048576U;

    inline volatile bool authority_ready{};
    inline volatile bool memory_ready{};
    inline volatile bool notification_ready{};
    inline volatile bool fault_ipc_observed{};
    inline volatile bool final_reported{};

    inline volatile u32 extent_node_failure_countdown{};

    inline void configure_extent_node_failure(u32 countdown) noexcept {
        __atomic_store_n(&extent_node_failure_countdown, countdown, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool fail_extent_node_allocation() noexcept {
        u32 value = __atomic_load_n(&extent_node_failure_countdown, __ATOMIC_ACQUIRE);
        for (;;) {
            if (value == 0U)
                return false;
            if (__atomic_compare_exchange_n(&extent_node_failure_countdown, &value, value - 1U,
                                            false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                return value == 1U;
        }
    }

    /*
     * Fault-injection sites (TST-024). See the production header for why
     * these name points that can already fail on their own.
     *
     * One countdown per site: configure_failure(site, n) makes the nth
     * subsequent check at that site report a failure and then disarm, so a
     * test injects exactly one failure into exactly one operation and the
     * system is left armed for nothing afterwards.
     */
    enum class injection_site : u32 {
        object_registration = 0U,
        capability_install = 1U,
        capability_mint = 3U,
        page_allocation = 2U,
        site_count = 4U,
    };

    inline volatile u32 injection_countdown[static_cast<u32>(injection_site::site_count)]{};

    inline void configure_failure(injection_site site, u32 countdown) noexcept {
        const u32 index = static_cast<u32>(site);
        if (index >= static_cast<u32>(injection_site::site_count))
            return;
        __atomic_store_n(&injection_countdown[index], countdown, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool fail(injection_site site) noexcept {
        const u32 index = static_cast<u32>(site);
        if (index >= static_cast<u32>(injection_site::site_count))
            return false;
        /*
         * Relaxed for the unarmed probe, which is every call in practice.
         * These sites sit on paths IPC transfer uses, and an acquire load
         * (ldar) there was measurable against the certification latency
         * bound. Nothing is ordered against an unarmed site by definition;
         * the compare-exchange below still carries acquire-release for the
         * arming thread.
         */
        u32 value = __atomic_load_n(&injection_countdown[index], __ATOMIC_RELAXED);
        for (;;) {
            if (value == 0U)
                return false;
            if (__atomic_compare_exchange_n(&injection_countdown[index], &value, value - 1U, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                return value == 1U;
        }
    }

    inline void report(const char* name, bool passed) noexcept {
        pr_info("[TEST] name=%s result=%s\n", name, passed ? "PASS" : "FAIL");
    }

    inline void mark_bootstrap_self_tests(bool authority, bool memory, bool notification) noexcept {
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
        pr_info(
            "[ACCEPTANCE] suite=root-only phase=runtime result=PASS operations=%llu failures=0\n",
            static_cast<unsigned long long>(operations));
        pr_info("[ACCEPTANCE] suite=root-only result=PASS failures=0\n");
    }
#else
    inline void mark_bootstrap_self_tests(bool, bool, bool) noexcept {}
    inline void mark_fault_ipc() noexcept {}
    inline void report_final(u64, u64, bool) noexcept {}
    enum class injection_site : u32 {
        object_registration = 0U,
        capability_install = 1U,
        page_allocation = 2U,
        capability_mint = 3U,
        site_count = 4U,
    };
    inline void configure_failure(injection_site, u32) noexcept {}
    [[nodiscard]] inline bool fail(injection_site) noexcept { return false; }
#endif
} // namespace sys::kernel::verification
