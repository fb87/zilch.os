#pragma once

#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::scheduling
{
    inline constexpr u8 lowest_priority = 0U;
    inline constexpr u8 highest_priority = 255U;
    inline constexpr u32 maximum_donation_depth = 8U;
    inline constexpr u64 maximum_time = ~0ULL;
    inline constexpr u32 maximum_replenishments = 16U;

    [[nodiscard]] inline constexpr bool deadline_fits(u64 now, u64 delay) noexcept {
        return delay <= maximum_time - now;
    }

    struct context {
        object::header_t object{};
        u8 priority{128U};
        u8 effective_priority{128U};
        u8 maximum_priority{highest_priority};
        u8 donation_depth{};
        u64 budget_ticks{1U};
        u64 period_ticks{1U};
        u64 consumed_ticks{};
        u64 donated_ticks{};
        u64 next_replenishment{};
        cpu_id_t affinity{};
        bool enabled{true};
        bool throttled{};
        u64 replenishment_deadlines[maximum_replenishments]{};
        u64 replenishment_amounts[maximum_replenishments]{};
        u32 replenishment_count{};
    };

    inline void initialize(context& value, cpu_id_t affinity) noexcept {
        value.priority = 128U;
        value.effective_priority = value.priority;
        value.maximum_priority = highest_priority;
        value.donation_depth = 0U;
        value.budget_ticks = 1U;
        value.period_ticks = 1U;
        value.consumed_ticks = 0U;
        value.donated_ticks = 0U;
        /*
         * maximum_time ("never due"), not 1: replenishment_count is 0 here,
         * and the invariant everywhere else in this file is that an empty
         * replenishment queue pairs with next_replenishment == maximum_time
         * (see replenish_if_due()'s own tail assignment and replenish()).
         * replenish_if_due() early-returns on an empty queue, so nothing
         * ever reads this value in that state -- but a freshly created
         * thread that is still alive and idle when the certification
         * harness runs scheduler_database_valid() would otherwise be
         * reported as an invalid context. Latent until thread_create made
         * it possible for a created-but-idle thread to still exist at
         * acceptance time; every previous thread was either busy enough to
         * have accumulated replenishments or destroyed before the check.
         */
        value.next_replenishment = maximum_time;
        value.affinity = affinity;
        value.enabled = true;
        value.throttled = false;
        value.replenishment_count = 0U;
    }

    [[nodiscard]] inline error_t configure(context& value, u8 priority, u64 budget, u64 period,
                                           cpu_id_t affinity, u64 now) noexcept {
        if (budget == 0U || period == 0U || budget > period || priority > value.maximum_priority) {
            return error_t::invalid_argument;
        }
        if (!deadline_fits(now, period))
            return error_t::invalid_argument;
        value.priority = priority;
        value.effective_priority = priority;
        value.budget_ticks = budget;
        value.period_ticks = period;
        value.consumed_ticks = 0U;
        value.donated_ticks = 0U;
        value.next_replenishment = now + period;
        value.affinity = affinity;
        value.enabled = true;
        value.throttled = false;
        value.donation_depth = 0U;
        value.replenishment_count = 0U;
        return error_t::success;
    }

    inline void insert_replenishment(context& value, u64 deadline, u64 amount) noexcept {
        if (amount == 0U)
            return;
        u32 position = 0U;
        while (position < value.replenishment_count &&
               value.replenishment_deadlines[position] < deadline)
            ++position;
        if (position < value.replenishment_count &&
            value.replenishment_deadlines[position] == deadline) {
            value.replenishment_amounts[position] += amount;
        } else if (value.replenishment_count < maximum_replenishments) {
            for (u32 index = value.replenishment_count; index > position; --index) {
                value.replenishment_deadlines[index] = value.replenishment_deadlines[index - 1U];
                value.replenishment_amounts[index] = value.replenishment_amounts[index - 1U];
            }
            value.replenishment_deadlines[position] = deadline;
            value.replenishment_amounts[position] = amount;
            ++value.replenishment_count;
        } else {
            /*
             * Coalesce overflow into the latest record. Delaying returned
             * budget is bandwidth-safe and, unlike dropping a record, always
             * permits a throttled context to make progress eventually.
             */
            const u32 last = maximum_replenishments - 1U;
            value.replenishment_amounts[last] += amount;
            if (deadline > value.replenishment_deadlines[last])
                value.replenishment_deadlines[last] = deadline;
        }
        value.next_replenishment = value.replenishment_deadlines[0];
    }

    inline void replenish_if_due(context& value, u64 now) noexcept {
        if (!value.enabled || value.period_ticks == 0U || value.replenishment_count == 0U) {
            return;
        }
        while (value.replenishment_count != 0U && value.replenishment_deadlines[0] <= now) {
            const u64 amount = value.replenishment_amounts[0];
            value.consumed_ticks =
                amount >= value.consumed_ticks ? 0U : value.consumed_ticks - amount;
            for (u32 index = 1U; index < value.replenishment_count; ++index) {
                value.replenishment_deadlines[index - 1U] = value.replenishment_deadlines[index];
                value.replenishment_amounts[index - 1U] = value.replenishment_amounts[index];
            }
            --value.replenishment_count;
        }
        value.next_replenishment =
            value.replenishment_count == 0U ? maximum_time : value.replenishment_deadlines[0];
        value.throttled = value.consumed_ticks >= value.budget_ticks;
    }

    [[nodiscard]] inline bool eligible(context& value, u64 now) noexcept {
        replenish_if_due(value, now);
        return value.enabled && (value.donated_ticks != 0U ||
                                 (!value.throttled && value.consumed_ticks < value.budget_ticks));
    }

    [[nodiscard]] inline bool charge(context& value, u64 now, u64 ticks) noexcept {
        if (!value.enabled)
            return false;
        replenish_if_due(value, now);
        if (value.donated_ticks != 0U) {
            const u64 charged = ticks < value.donated_ticks ? ticks : value.donated_ticks;
            value.donated_ticks -= charged;
            ticks -= charged;
            if (ticks == 0U)
                return true;
        }
        if (value.budget_ticks == 0U || value.throttled || ticks == 0U)
            return false;
        const u64 remaining = value.budget_ticks - value.consumed_ticks;
        const u64 charged = ticks < remaining ? ticks : remaining;
        value.consumed_ticks += charged;
        const u64 deadline =
            deadline_fits(now, value.period_ticks) ? now + value.period_ticks : maximum_time;
        insert_replenishment(value, deadline, charged);
        if (value.consumed_ticks >= value.budget_ticks)
            value.throttled = true;
        return ticks == charged && !value.throttled;
    }

    [[nodiscard]] inline bool charge(context& value, u64 ticks = 1U) noexcept {
        return charge(value, 0U, ticks);
    }

    inline void replenish(context& value) noexcept {
        value.consumed_ticks = 0U;
        value.throttled = false;
        value.replenishment_count = 0U;
        value.next_replenishment = maximum_time;
    }

    [[nodiscard]] inline error_t donate(context& receiver, context& donor, u64 now) noexcept {
        if (donor.donation_depth >= maximum_donation_depth || receiver.donation_depth != 0U ||
            receiver.donated_ticks != 0U)
            return error_t::busy;
        if (donor.effective_priority > receiver.effective_priority)
            receiver.effective_priority = donor.effective_priority;
        receiver.donation_depth = donor.donation_depth + 1U;
        const u64 own_remaining = donor.consumed_ticks < donor.budget_ticks
                                      ? donor.budget_ticks - donor.consumed_ticks
                                      : 0U;
        if (donor.donated_ticks > ~0ULL - own_remaining) {
            receiver.effective_priority = receiver.priority;
            receiver.donation_depth = 0U;
            return error_t::invalid_argument;
        }
        receiver.donated_ticks = donor.donated_ticks + own_remaining;
        const u64 deadline =
            deadline_fits(now, donor.period_ticks) ? now + donor.period_ticks : maximum_time;
        insert_replenishment(donor, deadline, own_remaining);
        donor.donated_ticks = 0U;
        donor.consumed_ticks = donor.budget_ticks;
        donor.throttled = true;
        return error_t::success;
    }

    [[nodiscard]] inline error_t donate(context& receiver, context& donor) noexcept {
        return donate(receiver, donor, 0U);
    }

    inline void revoke_donation(context& receiver, context& donor) noexcept {
        donor.donated_ticks += receiver.donated_ticks;
        receiver.donated_ticks = 0U;
        receiver.effective_priority = receiver.priority;
        receiver.donation_depth = 0U;
    }
} // namespace sys::kernel::scheduling

static_assert(sys::kernel::scheduling::deadline_fits(0U, ~0ULL));
static_assert(!sys::kernel::scheduling::deadline_fits(~0ULL, 1U));
