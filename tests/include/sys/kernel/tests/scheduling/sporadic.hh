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

        constexpr u64 logical_hours = 6U;
        constexpr u64 periods_per_hour = 3600U;
        kernel::scheduling::context soak{};
        kernel::scheduling::initialize(soak, 0U);
        if (kernel::scheduling::configure(soak, 200U, 4U, 1000U, 0U, 0U) != error_t::success)
            return error_t::invalid_argument;
        for (u64 period = 0U; period < logical_hours * periods_per_hour; ++period) {
            const u64 started = period * soak.period_ticks;
            if (!kernel::scheduling::charge(soak, started, 1U) ||
                !kernel::scheduling::charge(soak, started + 1U, 1U) ||
                !kernel::scheduling::charge(soak, started + 2U, 1U) ||
                kernel::scheduling::charge(soak, started + 3U, 1U) || !soak.throttled ||
                kernel::scheduling::eligible(soak, started + soak.period_ticks - 1U))
                return error_t::invalid_argument;
            kernel::scheduling::replenish_if_due(soak, started + soak.period_ticks + 3U);
            if (soak.throttled || soak.consumed_ticks != 0U || soak.replenishment_count != 0U ||
                !kernel::scheduling::eligible(soak, started + soak.period_ticks + 3U))
                return error_t::invalid_argument;
        }
        pr_info("[TEST] name=sporadic_server_replenishment result=PASS slices=3 queue=%u\n",
                static_cast<unsigned int>(kernel::scheduling::maximum_replenishments));
        pr_info("[TEST] name=rt_logical_time_soak result=PASS hours=%llu periods=%llu "
                "violations=0\n",
                static_cast<unsigned long long>(logical_hours),
                static_cast<unsigned long long>(logical_hours * periods_per_hour));
        return error_t::success;
    }
} // namespace sys::kernel::tests::scheduling
