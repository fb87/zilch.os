#pragma once

#include <sys/types.hh>

namespace sys::kernel::hypervisor
{
    inline constexpr u32 maximum_virtual_irqs = 64U;
    inline constexpr u8 default_virtual_irq_priority = 0x80U;

    enum class virtual_irq_class : u8 { sgi, ppi, spi };
    enum class virtual_irq_trigger : u8 { level, edge };

    [[nodiscard]] inline constexpr virtual_irq_class classify_virtual_irq(u16 irq) noexcept {
        return irq < 16U ? virtual_irq_class::sgi
                         : (irq < 32U ? virtual_irq_class::ppi : virtual_irq_class::spi);
    }

    struct virtual_interrupt_state {
        u64 pending{};
        u64 active{};
        u64 masked{};
        u64 level_asserted{};
        u64 edge_triggered{~0ULL};
        u8 priority[maximum_virtual_irqs]{};
        u8 priority_mask{0xffU};
        u8 running_priority{0xffU};
        u8 binary_point{};
        u16 last_acknowledged{static_cast<u16>(maximum_virtual_irqs)};
        u32 injections{};
        u32 acknowledgements{};
        u32 deactivations{};
        u32 maintenance_events{};
        bool hardware_active{};

        void initialize_priorities() noexcept {
            for (u32 index = 0U; index < maximum_virtual_irqs; ++index) {
                if (priority[index] == 0U)
                    priority[index] = default_virtual_irq_priority;
            }
        }

        [[nodiscard]] error_t configure(u16 irq, virtual_irq_trigger trigger,
                                        u8 irq_priority = default_virtual_irq_priority) noexcept {
            if (irq >= maximum_virtual_irqs || irq_priority == 0xffU)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if (trigger == virtual_irq_trigger::edge)
                edge_triggered |= bit;
            else
                edge_triggered &= ~bit;
            priority[irq] = irq_priority;
            return error_t::success;
        }

        [[nodiscard]] error_t inject(u16 irq, bool asserted = true) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            initialize_priorities();
            const u64 bit = 1ULL << irq;
            if ((edge_triggered & bit) == 0U) {
                if (asserted)
                    level_asserted |= bit;
                else {
                    level_asserted &= ~bit;
                    return error_t::success;
                }
            }
            if ((pending & bit) != 0U || (active & bit) != 0U)
                return error_t::busy;
            pending |= bit;
            ++injections;
            return error_t::success;
        }

        [[nodiscard]] error_t acknowledge(u16& irq) noexcept {
            const u64 deliverable = pending & ~masked;
            if (deliverable == 0U)
                return error_t::not_found;
            initialize_priorities();
            u8 selected_priority = 0xffU;
            irq = static_cast<u16>(maximum_virtual_irqs);
            for (u16 candidate = 0U; candidate < maximum_virtual_irqs; ++candidate) {
                const u64 bit = 1ULL << candidate;
                if ((deliverable & bit) == 0U || priority[candidate] >= priority_mask)
                    continue;
                if (irq == maximum_virtual_irqs || priority[candidate] < selected_priority ||
                    (priority[candidate] == selected_priority && candidate < irq)) {
                    irq = candidate;
                    selected_priority = priority[candidate];
                }
            }
            if (irq == maximum_virtual_irqs)
                return error_t::not_found;
            const u64 bit = 1ULL << irq;
            pending &= ~bit;
            active |= bit;
            running_priority = selected_priority;
            last_acknowledged = irq;
            ++acknowledgements;
            return error_t::success;
        }

        [[nodiscard]] error_t deactivate(u16 irq) noexcept {
            if (irq >= maximum_virtual_irqs)
                return error_t::invalid_argument;
            const u64 bit = 1ULL << irq;
            if ((active & bit) == 0U)
                return error_t::not_found;
            active &= ~bit;
            if ((edge_triggered & bit) == 0U && (level_asserted & bit) != 0U)
                pending |= bit;
            running_priority = 0xffU;
            ++deactivations;
            if (pending != 0U)
                ++maintenance_events;
            return error_t::success;
        }

        void set_priority_mask(u8 value) noexcept {
            priority_mask = value;
        }

        void mask(u16 irq, bool value) noexcept {
            if (irq >= maximum_virtual_irqs)
                return;
            const u64 bit = 1ULL << irq;
            if (value)
                masked |= bit;
            else
                masked &= ~bit;
        }

        void reset() noexcept {
            pending = 0U;
            active = 0U;
            masked = 0U;
            level_asserted = 0U;
            edge_triggered = ~0ULL;
            for (u32 index = 0U; index < maximum_virtual_irqs; ++index)
                priority[index] = default_virtual_irq_priority;
            priority_mask = 0xffU;
            running_priority = 0xffU;
            binary_point = 0U;
            last_acknowledged = static_cast<u16>(maximum_virtual_irqs);
            injections = 0U;
            acknowledgements = 0U;
            deactivations = 0U;
            maintenance_events = 0U;
            hardware_active = false;
        }
    };
} // namespace sys::kernel::hypervisor
