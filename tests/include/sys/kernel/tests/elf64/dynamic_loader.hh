#pragma once

#include <sys/arch/space/elf64_dynamic_check.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::elf64_dynamic_loader
{
    [[nodiscard]] inline error_t run_differential_check() noexcept {
        if (!arch::space::validate_elf64_dynamic_loader())
            return error_t::invalid_argument;

        pr_info("[TEST] name=elf64_dynamic_loader_differential result=PASS\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::elf64_dynamic_loader
