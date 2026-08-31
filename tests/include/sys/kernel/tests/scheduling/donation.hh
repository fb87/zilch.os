#pragma once

#include <sys/kernel/interrupt/timing.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>

namespace sys::kernel::tests::scheduling
{
    [[nodiscard]] inline error_t run_donation_chain() noexcept {
        kernel::scheduling::context donation_caller{};
        kernel::scheduling::context donation_middle{};
        kernel::scheduling::context donation_server{};
        kernel::scheduling::initialize(donation_caller, 0U);
        kernel::scheduling::initialize(donation_middle, 0U);
        kernel::scheduling::initialize(donation_server, 0U);
        error_t result = kernel::scheduling::configure(donation_caller, 240U, 10U, 20U, 0U, 0U);
        if (result == error_t::success)
            result = kernel::scheduling::configure(donation_middle, 80U, 5U, 20U, 0U, 0U);
        if (result == error_t::success)
            result = kernel::scheduling::configure(donation_server, 20U, 4U, 20U, 0U, 0U);
        if (result != error_t::success || !kernel::scheduling::charge(donation_caller, 2U) ||
            kernel::scheduling::donate(donation_middle, donation_caller) != error_t::success ||
            donation_middle.effective_priority != 240U || donation_middle.donated_ticks != 8U ||
            !kernel::scheduling::charge(donation_middle, 3U) ||
            kernel::scheduling::donate(donation_server, donation_middle) != error_t::success ||
            donation_server.donation_depth != 2U || donation_server.effective_priority != 240U ||
            donation_server.donated_ticks != 10U ||
            !kernel::scheduling::charge(donation_server, 4U))
            return error_t::invalid_argument;
        kernel::scheduling::revoke_donation(donation_server, donation_middle);
        kernel::scheduling::revoke_donation(donation_middle, donation_caller);
        donation_middle.donation_depth = kernel::scheduling::maximum_donation_depth;
        if (donation_caller.donated_ticks != 6U ||
            kernel::scheduling::donate(donation_server, donation_middle) != error_t::busy ||
            !kernel::scheduling::eligible(donation_caller, 0U) ||
            !kernel::scheduling::charge(donation_caller, 6U))
            return error_t::invalid_argument;
        pr_info("[TEST] name=scheduling_context_donation result=PASS depth=2 returned=6\n");
        pr_info("[TEST] name=priority_inheritance result=PASS inherited=240 base=20\n");
        pr_info("[TEST] name=donation_chain_bound result=PASS maximum=8\n");
        return error_t::success;
    }

    [[nodiscard]] inline error_t run_lock_order_check() noexcept {
        if (lock_order::violation_count() != 0U)
            return error_t::invalid_argument;
        pr_info("[TEST] name=lock_hold_measurement result=PASS max_ticks=%llu\n",
                static_cast<unsigned long long>(lock_order::maximum_hold()));
        pr_info("[METRIC] name=irq_disabled_duration max_ticks=%llu reference_ticks=%llu "
                "samples=%llu\n",
                static_cast<unsigned long long>(kernel::interrupt::timing::maximum()),
                static_cast<unsigned long long>(kernel::interrupt::timing::target_ticks()),
                static_cast<unsigned long long>(kernel::interrupt::timing::sample_count()));
        return error_t::success;
    }
} // namespace sys::kernel::tests::scheduling
