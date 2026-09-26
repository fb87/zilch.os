#pragma once

#include <sys/types.hh>

namespace sys::kernel::verification
{
    inline void mark_bootstrap_self_tests(bool, bool, bool) noexcept {}
    inline void mark_fault_ipc() noexcept {}
    inline void report_final(u64, u64, bool) noexcept {}
    [[nodiscard]] inline bool fail_extent_node_allocation() noexcept {
        return false;
    }
    inline void configure_extent_node_failure(u32) noexcept {}

    /*
     * Fault-injection sites (TST-024).
     *
     * Each names a point where an operation can already fail for real --
     * a full object table, an exhausted allocator, an occupied capability
     * slot -- so injecting exercises rollback paths that must exist
     * anyway, rather than inventing new ones. Production builds compile
     * every check to a constant false; the certification tree shadows this
     * header with a counting implementation.
     */
    enum class injection_site : u32 {
        object_registration = 0U,
        capability_install = 1U,
        capability_mint = 3U,
        page_allocation = 2U,
        site_count = 4U,
    };

    inline void configure_failure(injection_site, u32) noexcept {}
    [[nodiscard]] inline bool fail(injection_site) noexcept {
        return false;
    }
} // namespace sys::kernel::verification
