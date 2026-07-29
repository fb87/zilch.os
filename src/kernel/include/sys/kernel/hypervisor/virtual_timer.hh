#pragma once

#include <sys/types.hh>

namespace sys::kernel::hypervisor
{
    struct virtual_timer_state {
        u64 deadline{};
        u64 control{};
        u32 generation{};
        u32 expirations{};
        u32 cancellations{};
        bool armed{};
        bool pending{};

        void reset() noexcept {
            deadline = 0U;
            control = 0U;
            generation = 0U;
            expirations = 0U;
            cancellations = 0U;
            armed = false;
            pending = false;
        }

        void synchronize(u64 new_control, u64 compare) noexcept {
            const bool enabled = (new_control & 1U) != 0U;
            if (armed && !enabled)
                ++cancellations;
            control = new_control & 0x7U;
            deadline = compare;
            armed = enabled;
            if (!enabled)
                pending = false;
            ++generation;
        }

        [[nodiscard]] bool expire(u64 virtual_now) noexcept {
            const bool masked = (control & (1U << 1U)) != 0U;
            if (!armed || masked || pending || virtual_now < deadline)
                return false;
            armed = false;
            pending = true;
            ++expirations;
            return true;
        }

        void acknowledge() noexcept {
            pending = false;
        }
    };

    [[nodiscard]] inline constexpr u64 virtual_counter(u64 physical_counter,
                                                       u64 counter_offset) noexcept {
        return physical_counter - counter_offset;
    }
} // namespace sys::kernel::hypervisor
