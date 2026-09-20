#pragma once

#include <sys/kernel/hypervisor/virtual_irq.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::virtual_irq_storm
{
    /*
     * Virtual interrupt storm behaviour.
     *
     * Worth being precise about what is being tested, because the obvious
     * reading of "storm test" does not apply here. There is no rate limiter
     * to exercise: this controller's defence is structural. Pending, active,
     * masked and level state are all single bits in a 64-bit word, so a
     * guest device injecting the same interrupt a million times sets one bit
     * a million times. Nothing queues, so nothing can be made to overflow.
     *
     * That is the property to pin, and the way to break it is not a flood --
     * it is a bookkeeping slip under one, where a repeated injection leaks a
     * bit into `active`, or an acknowledge loses one, or a level-triggered
     * line stops re-pending after deactivation. So this storms the
     * controller hard and then checks the state word is still exactly what
     * it should be.
     *
     * Deliberately NOT modelled on the physical storm detector: that one
     * counts deliveries against a window and is, as recorded in checklist
     * 0158, not in force at all. Writing a virtual counterpart shaped like
     * it would add a second test with the same blind spot.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        using namespace sys::kernel::hypervisor;
        static virtual_interrupt_state state{};
        state.pending = 0U;
        state.active = 0U;
        state.masked = 0U;
        state.level_asserted = 0U;
        state.edge_triggered = ~0ULL;
        state.priority_mask = 0xffU;
        state.running_priority = 0xffU;
        state.injections = 0U;
        state.acknowledgements = 0U;
        state.deactivations = 0U;

        constexpr u16 irq = 7U;
        const u64 bit = 1ULL << irq;

        if (state.configure(irq, virtual_irq_trigger::edge) != error_t::success)
            return error_t::invalid_argument;

        // Out of range is rejected rather than wrapping into another line's
        // bit, which a shift by 64 would do.
        u16 scratch = 0U;
        if (state.inject(static_cast<u16>(maximum_virtual_irqs)) != error_t::invalid_argument ||
            state.deactivate(static_cast<u16>(maximum_virtual_irqs)) != error_t::invalid_argument)
            return error_t::invalid_argument;

        /*
         * The storm. First injection takes, every one after it is refused as
         * busy while the bit is still pending -- which is the saturation
         * that makes a flood harmless -- and the injection counter must
         * record exactly the one that took.
         */
        if (state.inject(irq) != error_t::success)
            return error_t::invalid_argument;
        for (u32 attempt = 0U; attempt < 4096U; ++attempt) {
            if (state.inject(irq) != error_t::busy)
                return error_t::invalid_argument;
        }
        if (state.pending != bit || state.active != 0U || state.injections != 1U)
            return error_t::invalid_argument;

        // Acknowledge moves the bit pending -> active, exactly once.
        if (state.acknowledge(scratch) != error_t::success || scratch != irq)
            return error_t::invalid_argument;
        if (state.pending != 0U || state.active != bit || state.acknowledgements != 1U)
            return error_t::invalid_argument;

        // Storming an ALREADY ACTIVE line must not re-pend it: a guest that
        // has not finished servicing an interrupt cannot be made to nest
        // into itself by injecting harder.
        for (u32 attempt = 0U; attempt < 4096U; ++attempt) {
            if (state.inject(irq) != error_t::busy)
                return error_t::invalid_argument;
        }
        if (state.pending != 0U || state.active != bit || state.injections != 1U)
            return error_t::invalid_argument;

        if (state.deactivate(irq) != error_t::success)
            return error_t::invalid_argument;
        if (state.pending != 0U || state.active != 0U)
            return error_t::invalid_argument; // edge-triggered: nothing re-pends

        /*
         * Level-triggered is the case that must behave differently: while
         * the line is still asserted, deactivation re-pends it. A controller
         * that dropped it here would lose a still-active device condition;
         * one that re-pent an edge line would spin forever.
         */
        if (state.configure(irq, virtual_irq_trigger::level) != error_t::success)
            return error_t::invalid_argument;
        if (state.inject(irq, true) != error_t::success)
            return error_t::invalid_argument;
        if (state.acknowledge(scratch) != error_t::success || scratch != irq)
            return error_t::invalid_argument;
        if (state.deactivate(irq) != error_t::success)
            return error_t::invalid_argument;
        if (state.pending != bit)
            return error_t::invalid_argument; // still asserted, so still pending

        // Dropping the level then deactivating must clear it for good.
        if (state.inject(irq, false) != error_t::success)
            return error_t::invalid_argument;
        if (state.acknowledge(scratch) != error_t::success)
            return error_t::invalid_argument;
        if (state.deactivate(irq) != error_t::success)
            return error_t::invalid_argument;
        if (state.pending != 0U || state.active != 0U)
            return error_t::invalid_argument;

        /*
         * Saturate every line at once, then drain. Sixty-four simultaneous
         * storms must leave a full pending word and drain to exactly empty,
         * with no bit stuck in active -- the shape a leak would take.
         */
        state.priority_mask = 0xffU;
        for (u16 line = 0U; line < maximum_virtual_irqs; ++line) {
            if (state.configure(line, virtual_irq_trigger::edge) != error_t::success)
                return error_t::invalid_argument;
            if (state.inject(line) != error_t::success)
                return error_t::invalid_argument;
        }
        if (state.pending != ~0ULL)
            return error_t::invalid_argument;

        for (u16 drained = 0U; drained < maximum_virtual_irqs; ++drained) {
            if (state.acknowledge(scratch) != error_t::success)
                return error_t::invalid_argument;
            if (state.deactivate(scratch) != error_t::success)
                return error_t::invalid_argument;
        }
        if (state.pending != 0U || state.active != 0U)
            return error_t::invalid_argument;
        if (state.acknowledge(scratch) != error_t::not_found)
            return error_t::invalid_argument; // drained means drained

        pr_info("[TEST] name=virtual_irq_storm result=PASS saturating=1 "
                "no_reinject_while_active=1 level_repends=1 edge_does_not=1 "
                "full_drain=1 lines=%u\n",
                static_cast<unsigned int>(maximum_virtual_irqs));
        return error_t::success;
    }
} // namespace sys::kernel::tests::virtual_irq_storm
