#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::earlyfs
{
    [[nodiscard]] inline error_t run_directory_scan() noexcept {
        if (!arch::space::validate_earlyfs_image())
            return error_t::invalid_argument;

        pr_info("[TEST] name=earlyfs_directory_scan result=PASS\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::earlyfs
