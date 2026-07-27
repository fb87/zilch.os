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
} // namespace sys::kernel::verification
