#pragma once

#include <sys/types.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u32 maximum_virtual_irqs = 64U;

    struct virtual_interrupt_state {
        u64 pending{};
        u64 active{};
        u64 masked{};

        [[nodiscard]] error_t inject(u16 irq) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if ((pending & bit) != 0U || (active & bit) != 0U)
                return error_t::busy;
            pending |= bit;
            return error_t::success;
        }

        [[nodiscard]] error_t acknowledge(u16& irq) noexcept {
            const u64 deliverable = pending & ~masked;
            if (deliverable == 0U)
                return error_t::not_found;
            irq = static_cast<u16>(__builtin_ctzll(deliverable));
            const u64 bit = 1ULL << irq;
            pending &= ~bit;
            active |= bit;
            return error_t::success;
        }

        [[nodiscard]] error_t deactivate(u16 irq) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if ((active & bit) == 0U)
                return error_t::not_found;
            active &= ~bit;
            return error_t::success;
        }

        void reset() noexcept {
            pending = 0U;
            active = 0U;
            masked = 0U;
        }
    };
} // namespace sys::kernel::hypervisor
