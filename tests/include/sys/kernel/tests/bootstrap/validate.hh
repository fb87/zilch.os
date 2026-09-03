#pragma once

/*
 * The kernel bootstrap self-test harness -- exercised only by CONFIG_SELFTEST
 * builds, never by production boot. Extracted out of
 * sys/kernel/thread/scheduler.hh (a production file), which now just calls
 * tests::self_test::validate() under #if CONFIG_SELFTEST before continuing
 * its normal boot sequence.
 */

#include <sys/kernel/thread/scheduler.hh>

/*
 * Gated on __aarch64__, not just CONFIG_SELFTEST: these modules are written
 * against arm64-specific state (context.x[] register arrays, ASID fields,
 * arch::hypervisor's real guest-mode internals) that amd64 doesn't share --
 * porting the self-test suite itself is tracked as Phase 12 of the amd64
 * parity plan, not a header-selection problem polymorphism can paper over.
 * validate()'s amd64 branch below calls none of these.
 */
#if defined(__aarch64__)
#if CONFIG_HYPERVISOR_SELFTEST
#include <sys/kernel/tests/hypervisor/control_models.hh>
#endif
#if CONFIG_SELFTEST
#include <sys/kernel/tests/capability/derivation.hh>
#include <sys/kernel/tests/capability/scale.hh>
#include <sys/kernel/tests/capability/selector.hh>
#include <sys/kernel/tests/earlyfs/directory.hh>
#include <sys/kernel/tests/earlyfs/role_binding.hh>
#include <sys/kernel/tests/elf64/dynamic_loader.hh>
#include <sys/kernel/tests/fault/lifecycle.hh>
#include <sys/kernel/tests/fault/reply_wakeup.hh>
#include <sys/kernel/tests/hardening/boot_checks.hh>
#include <sys/kernel/tests/interrupt/lifecycle.hh>
#include <sys/kernel/tests/ipc/badge_delivery.hh>
#include <sys/kernel/tests/memory/physical_region_layout.hh>
#include <sys/kernel/tests/object/generation.hh>
#include <sys/kernel/tests/scheduling/donation.hh>
#include <sys/kernel/tests/scheduling/sporadic.hh>
#include <sys/kernel/tests/space/asid_rollover.hh>
#endif
#endif

namespace sys::kernel::tests::self_test
{
    [[nodiscard]] inline error_t validate() noexcept {
#if defined(__aarch64__)
        if (tests::scheduling::run_sporadic_server() != error_t::success)
            return error_t::invalid_argument;
        if (!arch::memory::kernel_stack_guards_valid())
            return error_t::invalid_argument;
        if (!arch::memory::kernel_permissions_valid())
            return error_t::invalid_argument;
        if (!emergency::verify_ring())
            return error_t::invalid_argument;
        if (!arch::memory::privilege_protection_enabled())
            return error_t::invalid_argument;
        if (!memory::verify_page_reuse_scrubbing())
            return error_t::invalid_argument;
        if (tests::space::run_rollover_reuse(thread::user_threads[0]) != error_t::success)
            return error_t::invalid_argument;

        if (tests::physical_memory::run_layout_check() != error_t::success)
            return error_t::invalid_argument;

        task::task& root = thread::user_tasks[0];
        error_t result = tests::capability_scale::run_basic_lifecycle(root);
        if (result != error_t::success)
            return result;

        result = tests::capability_derivation::run_derivation_generation_aba(root);
        if (result != error_t::success)
            return result;
        result = tests::capability_selector::run_selector_width_negative(root);
        if (result != error_t::success)
            return result;
        result = tests::object_generation::run_generation_retirement();
        if (result != error_t::success)
            return result;
        result = tests::earlyfs::run_directory_scan();
        if (result != error_t::success)
            return result;
        result = tests::elf64_dynamic_loader::run_differential_check();
        if (result != error_t::success)
            return result;
        result = tests::earlyfs::run_role_binding_check(root);
        if (result != error_t::success)
            return result;

        static capability::cspace_t guarded_cspace{};
        result = tests::capability_scale::run_guarded_scale_and_fuzz(root, guarded_cspace);
        if (result != error_t::success)
            return result;

        result = tests::scheduling::run_donation_chain();
        if (result != error_t::success)
            return result;
        result = tests::scheduling::run_lock_order_check();
        if (result != error_t::success)
            return result;

        result = tests::interrupt::run(root, guarded_cspace);
        if (result != error_t::success)
            return result;
        result = tests::ipc::run_badge_delivery(root);
        if (result != error_t::success)
            return result;

        result = tests::fault_lifecycle::run(root);
        if (result != error_t::success)
            return result;

        result = tests::fault_lifecycle::run_reply_wakeup(root);
        if (result != error_t::success)
            return result;

        notification::signal(bootstrap::root_notification, 1U);
        if (notification::consume(bootstrap::root_notification) != 1U)
            return error_t::invalid_argument;
        verification::mark_bootstrap_self_tests(true, true, true);
        if constexpr (arch::hypervisor::active) {
            result = hypervisor::test::run_all();
            if (result != error_t::success)
                return result;
            pr_info("[HV-DIAG] suite=single-vcpu self-test=PASS operations=%llu failures=%llu\n",
                    static_cast<unsigned long long>(hypervisor::test::operations),
                    static_cast<unsigned long long>(hypervisor::test::failures_total));
        }
        result = tests::hardening::run_user_range_and_arch_check();
        if (result != error_t::success)
            return result;
        result = tests::hardening::run_independent_panic_check();
        if (result != error_t::success)
            return result;
        return error_t::success;
#else
        /* amd64: selftest harness will be added in Phase 12 once architecture features are complete
         */
        return error_t::success;
#endif
    }
} // namespace sys::kernel::tests::self_test
