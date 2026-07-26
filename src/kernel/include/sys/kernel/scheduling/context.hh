#pragma once

#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::scheduling
{
    struct context {
        object::header_t object{};
        u8 priority{128U};
        u8 maximum_priority{255U};
        u16 reserved{};
        u64 budget_ticks{1U};
        u64 period_ticks{1U};
        u64 consumed_ticks{};
        cpu_id_t affinity{};
        bool enabled{true};
    };

    inline void initialize(context& value, cpu_id_t affinity) noexcept {
        value.priority = 128U;
        value.maximum_priority = 255U;
        value.budget_ticks = 1U;
        value.period_ticks = 1U;
        value.consumed_ticks = 0U;
        value.affinity = affinity;
        value.enabled = true;
    }

    [[nodiscard]] inline bool charge(context& value, u64 ticks = 1U) noexcept {
        if (!value.enabled || value.budget_ticks == 0U)
            return false;
        value.consumed_ticks += ticks;
        return value.consumed_ticks <= value.budget_ticks;
    }

    inline void replenish(context& value) noexcept {
        value.consumed_ticks = 0U;
    }
} // namespace sys::kernel::scheduling
