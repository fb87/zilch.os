#pragma once

#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduling/context.hh>

namespace sys::kernel::tests::scheduling_configure
{
    /*
     * Randomised contract check on scheduling context configuration, which
     * is the operation a thread migration goes through.
     *
     * Migrating a thread rewrites its priority, budget, replenishment and
     * affinity together. `scheduling_configure` will only do that to a
     * SUSPENDED thread holding no reply authority and no donated budget --
     * the reason given in control.hh being that a remote scheduler tick
     * must never observe a partially rewritten context. That argument only
     * holds if configuration is genuinely all-or-nothing, so this asserts
     * exactly that, over several thousand generated inputs.
     *
     * The acceptance rule is checked against the CONTRACT rather than
     * against what the code happens to do: an input is valid precisely when
     * budget and period are non-zero, budget fits within period, priority
     * is within the context's maximum, and the resulting deadline does not
     * overflow. Deriving the expectation independently is what makes this a
     * test rather than a restatement.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        using namespace sys::kernel::scheduling;
        static context value{};
        initialize(value, static_cast<cpu_id_t>(0U));

        constexpr u32 iterations = 4096U;
        u64 state = 0x243f6a8885a308d3ULL; // fixed seed: failures must reproduce
        u32 accepted = 0U;
        u32 rejected = 0U;

        for (u32 index = 0U; index < iterations; ++index) {
            const auto next = [&state]() noexcept {
                state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                return state >> 33U;
            };

            // Deliberately skewed towards the boundaries, where the
            // interesting cases live: zero, equal, off-by-one, and overflow.
            const u8 priority = static_cast<u8>(next() & 0xffU);
            const u64 budget = next() % 5U == 0U ? 0U : (next() % 64U);
            const u64 period = next() % 5U == 0U ? 0U : (next() % 64U);
            const auto affinity = static_cast<cpu_id_t>(next() % 4U);
            const u64 now = next() % 3U == 0U ? ~0ULL - (next() % 8U) : (next() % 4096U);

            // Snapshot every field configuration is allowed to touch.
            const u8 prior_priority = value.priority;
            const u8 prior_effective = value.effective_priority;
            const u64 prior_budget = value.budget_ticks;
            const u64 prior_period = value.period_ticks;
            const u64 prior_consumed = value.consumed_ticks;
            const u64 prior_donated = value.donated_ticks;
            const u64 prior_next = value.next_replenishment;
            const cpu_id_t prior_affinity = value.affinity;
            const bool prior_enabled = value.enabled;
            const bool prior_throttled = value.throttled;
            const u32 prior_depth = value.donation_depth;
            const u32 prior_count = value.replenishment_count;

            const bool expected_valid = budget != 0U && period != 0U && budget <= period &&
                                        priority <= value.maximum_priority &&
                                        deadline_fits(now, period);
            const error_t result = configure(value, priority, budget, period, affinity, now);

            if ((result == error_t::success) != expected_valid)
                return error_t::invalid_argument; // acceptance disagrees with the contract

            if (result != error_t::success) {
                ++rejected;
                // Rejection must be total: not one field moved.
                if (value.priority != prior_priority || value.effective_priority != prior_effective ||
                    value.budget_ticks != prior_budget || value.period_ticks != prior_period ||
                    value.consumed_ticks != prior_consumed ||
                    value.donated_ticks != prior_donated ||
                    value.next_replenishment != prior_next || value.affinity != prior_affinity ||
                    value.enabled != prior_enabled || value.throttled != prior_throttled ||
                    value.donation_depth != prior_depth ||
                    value.replenishment_count != prior_count)
                    return error_t::invalid_argument;
                continue;
            }

            ++accepted;
            /*
             * Acceptance must be equally total. A migration that set the new
             * affinity but left stale consumed budget or a donation depth
             * behind would hand the destination CPU a context describing a
             * thread that no longer exists in that form.
             */
            if (value.priority != priority || value.effective_priority != priority ||
                value.budget_ticks != budget || value.period_ticks != period ||
                value.affinity != affinity || value.next_replenishment != now + period)
                return error_t::invalid_argument;
            if (value.consumed_ticks != 0U || value.donated_ticks != 0U ||
                value.donation_depth != 0U || value.replenishment_count != 0U)
                return error_t::invalid_argument;
            if (!value.enabled || value.throttled)
                return error_t::invalid_argument;
        }

        // A run that only ever rejected, or only ever accepted, proves
        // nothing about the boundary it is meant to be probing.
        if (accepted == 0U || rejected == 0U)
            return error_t::invalid_argument;

        pr_info("[TEST] name=scheduling_configure_fuzz result=PASS iterations=%u accepted=%u "
                "rejected=%u transactional=1\n",
                iterations, accepted, rejected);
        return error_t::success;
    }
} // namespace sys::kernel::tests::scheduling_configure
