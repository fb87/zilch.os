#pragma once

#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::scheduling
{
    inline constexpr u8 lowest_priority = 0U;
    inline constexpr u8 highest_priority = 255U;
    inline constexpr u32 maximum_donation_depth = 8U;

    struct context {
        object::header_t object{};
        u8 priority{128U};
        u8 effective_priority{128U};
        u8 maximum_priority{highest_priority};
        u8 donation_depth{};
        u64 budget_ticks{1U};
        u64 period_ticks{1U};
        u64 consumed_ticks{};
        u64 next_replenishment{};
        cpu_id_t affinity{};
        bool enabled{true};
        bool throttled{};
    };

    inline void initialize(context& value, cpu_id_t affinity) noexcept {
        value.priority = 128U;
        value.effective_priority = value.priority;
        value.maximum_priority = highest_priority;
        value.donation_depth = 0U;
        value.budget_ticks = 1U;
        value.period_ticks = 1U;
        value.consumed_ticks = 0U;
        value.next_replenishment = 1U;
        value.affinity = affinity;
        value.enabled = true;
        value.throttled = false;
    }

    [[nodiscard]] inline error_t configure(context& value, u8 priority, u64 budget, u64 period,
                                           cpu_id_t affinity, u64 now) noexcept {
        if (budget == 0U || period == 0U || budget > period || priority > value.maximum_priority) {
            return error_t::invalid_argument;
        }
        value.priority = priority;
        value.effective_priority = priority;
        value.budget_ticks = budget;
        value.period_ticks = period;
        value.consumed_ticks = 0U;
        value.next_replenishment = now + period;
        value.affinity = affinity;
        value.enabled = true;
        value.throttled = false;
        value.donation_depth = 0U;
        return error_t::success;
    }

    inline void replenish_if_due(context& value, u64 now) noexcept {
        if (!value.enabled || value.period_ticks == 0U || now < value.next_replenishment) {
            return;
        }
        const u64 elapsed = now - value.next_replenishment;
        const u64 periods = (elapsed / value.period_ticks) + 1U;
        value.next_replenishment += periods * value.period_ticks;
        value.consumed_ticks = 0U;
        value.throttled = false;
    }

    [[nodiscard]] inline bool eligible(context& value, u64 now) noexcept {
        replenish_if_due(value, now);
        return value.enabled && !value.throttled && value.consumed_ticks < value.budget_ticks;
    }

    [[nodiscard]] inline bool charge(context& value, u64 ticks = 1U) noexcept {
        if (!value.enabled || value.budget_ticks == 0U || value.throttled)
            return false;
        if (ticks > value.budget_ticks - value.consumed_ticks) {
            value.consumed_ticks = value.budget_ticks;
            value.throttled = true;
            return false;
        }
        value.consumed_ticks += ticks;
        if (value.consumed_ticks >= value.budget_ticks)
            value.throttled = true;
        return !value.throttled;
    }

    inline void replenish(context& value) noexcept {
        value.consumed_ticks = 0U;
        value.throttled = false;
    }

    [[nodiscard]] inline error_t donate_priority(context& receiver, const context& donor) noexcept {
        if (receiver.donation_depth >= maximum_donation_depth)
            return error_t::busy;
        if (donor.effective_priority > receiver.effective_priority)
            receiver.effective_priority = donor.effective_priority;
        ++receiver.donation_depth;
        return error_t::success;
    }

    inline void revoke_donation(context& value) noexcept {
        value.effective_priority = value.priority;
        value.donation_depth = 0U;
    }
} // namespace sys::kernel::scheduling
