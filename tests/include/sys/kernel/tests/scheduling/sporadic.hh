#pragma once

#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>

namespace sys::kernel::tests::scheduling
{
    [[nodiscard]] inline error_t run_sporadic_server() noexcept {
        kernel::scheduling::context sporadic{};
        kernel::scheduling::initialize(sporadic, 0U);
        if (kernel::scheduling::configure(sporadic, 128U, 6U, 10U, 0U, 100U) != error_t::success ||
            !kernel::scheduling::charge(sporadic, 100U, 2U) ||
            !kernel::scheduling::charge(sporadic, 103U, 3U) ||
            kernel::scheduling::charge(sporadic, 104U, 1U) ||
            kernel::scheduling::eligible(sporadic, 109U))
            return error_t::invalid_argument;
        kernel::scheduling::replenish_if_due(sporadic, 110U);
        if (sporadic.consumed_ticks != 4U || !kernel::scheduling::eligible(sporadic, 110U))
            return error_t::invalid_argument;
        kernel::scheduling::replenish_if_due(sporadic, 113U);
        if (sporadic.consumed_ticks != 1U)
            return error_t::invalid_argument;
        kernel::scheduling::replenish_if_due(sporadic, 114U);
        if (sporadic.consumed_ticks != 0U || sporadic.replenishment_count != 0U ||
            sporadic.next_replenishment != kernel::scheduling::maximum_time ||
            kernel::scheduling::configure(sporadic, 128U, 1U, 1U, 0U,
                                          kernel::scheduling::maximum_time) !=
                error_t::invalid_argument)
            return error_t::invalid_argument;

        kernel::scheduling::context bounded{};
        kernel::scheduling::initialize(bounded, 0U);
        if (kernel::scheduling::configure(bounded, 128U, 20U, 100U, 0U, 0U) != error_t::success)
            return error_t::invalid_argument;
        for (u64 tick = 0U; tick < 20U; ++tick)
            (void)kernel::scheduling::charge(bounded, tick, 1U);
        if (!bounded.throttled ||
            bounded.replenishment_count != kernel::scheduling::maximum_replenishments)
            return error_t::invalid_argument;
        kernel::scheduling::replenish_if_due(bounded, 119U);
        if (bounded.throttled || bounded.consumed_ticks != 0U || bounded.replenishment_count != 0U)
            return error_t::invalid_argument;
        pr_info("[TEST] name=sporadic_server_replenishment result=PASS slices=3 queue=%u\n",
                static_cast<unsigned int>(kernel::scheduling::maximum_replenishments));
        return error_t::success;
    }
} // namespace sys::kernel::tests::scheduling
