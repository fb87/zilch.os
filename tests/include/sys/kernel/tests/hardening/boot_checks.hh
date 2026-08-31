#pragma once

/*
 * NOT a standalone-includable header: like fault/lifecycle.hh, this is
 * #include'd from a specific point inside
 * sys::kernel::thread::scheduler.hh's own body (see that file's
 * validate_bootstrap_objects()), after user_threads[], current_user_thread[],
 * classify_user_fault(), and user_thread_count have already been declared
 * there -- all sys::kernel::thread-internal symbols with no home outside
 * that file.
 */
namespace sys::kernel::tests::hardening
{
    [[nodiscard]] inline error_t run_user_range_and_arch_check() noexcept {
        const auto& root_space = thread::user_threads[0].address_space.native;
        if (!user_access::valid_range(root_space, arch::space::user_code, 4U, false) ||
            user_access::valid_range(root_space, arch::space::user_code, 4U, true) ||
            !user_access::valid_range(root_space, arch::space::user_stack_base, 16U, true) ||
            user_access::valid_range(root_space, arch::space::kernel_identity_base - 1U, 2U,
                                     false) ||
            user_access::valid_range(root_space, ~static_cast<vaddr_t>(0U) - 1U, 4U, false) ||
            user_access::valid_range(root_space, arch::space::user_image_end() + 0x1000U, 16U,
                                     false) ||
            thread::classify_user_fault(0x00U) != fault::kind::instruction_abort ||
            thread::classify_user_fault(0x24U) != fault::kind::data_abort ||
            thread::classify_user_fault(0x3fU) != fault::kind::none ||
            !arch::hardening::inventory_valid(arch::smp::online_count()))
            return error_t::invalid_argument;
        pr_info("[TEST] name=user_range_and_arm_hardening result=PASS\n");
        return error_t::success;
    }

    [[nodiscard]] inline error_t run_independent_panic_check() noexcept {
        const cpu_id_t panic_cpu = arch::cpu::current_id();
        const u32 saved_current = thread::current_user_thread[panic_cpu];
        const u32 saved_printk_lock = ::sys::printk::raw_lock;
        thread::current_user_thread[panic_cpu] = thread::user_thread_count + 1U;
        ::sys::printk::raw_lock = 1U;
        panic::capture(panic::reason::internal_failure, 1U, 0xf0U, 0xdeadU, 0xbeefU, 0xcafef00dU);
        ::sys::printk::raw_lock = saved_printk_lock;
        thread::current_user_thread[panic_cpu] = saved_current;
        if (!emergency::crash_valid() || emergency::preserved_crash.level != 1U ||
            emergency::preserved_crash.vector != 0xf0U ||
            emergency::preserved_crash.syndrome != 0xdeadU ||
            emergency::preserved_crash.fault_address != 0xbeefU ||
            emergency::preserved_crash.instruction_pointer != 0xcafef00dU)
            return error_t::invalid_argument;
        pr_info("[TEST] name=scheduler_independent_panic result=PASS\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::hardening
