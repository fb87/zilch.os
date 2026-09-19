#pragma once

#include <sys/kernel/bootstrap.hh>
#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>

namespace sys::kernel::tests::interrupt
{
    [[nodiscard]] inline error_t run(task::task& root,
                                     capability::cspace_t& delegated_cspace) noexcept {
        static kernel::interrupt::interrupt_t irq{};
        static kernel::interrupt::interrupt_t level_irq{};
        kernel::interrupt::interrupt_t duplicate{};
        kernel::interrupt::interrupt_t reserved{};
        kernel::interrupt::initialize(irq, 40U, kernel::interrupt::trigger::edge);
        kernel::interrupt::initialize(level_irq, 41U, kernel::interrupt::trigger::level);
        kernel::interrupt::initialize(duplicate, 40U, kernel::interrupt::trigger::edge);
        kernel::interrupt::initialize(reserved, platform::interrupt::virtual_timer_irq,
                                      kernel::interrupt::trigger::level);
        error_t result = object::register_dynamic_object(irq.object, object::type_t::interrupt);
        if (result == error_t::success)
            result = kernel::interrupt::register_irq(irq);
        if (result == error_t::success &&
            (kernel::interrupt::register_irq(duplicate) != error_t::busy ||
             kernel::interrupt::register_irq(reserved) != error_t::busy))
            result = error_t::invalid_argument;
        if (result == error_t::success)
            result = object::register_dynamic_object(level_irq.object, object::type_t::interrupt);
        if (result == error_t::success)
            result = kernel::interrupt::register_irq(level_irq);
        const capability::rights_t owner_rights{static_cast<u32>(capability::right_t::read) |
                                                static_cast<u32>(capability::right_t::write) |
                                                static_cast<u32>(capability::right_t::grant) |
                                                static_cast<u32>(capability::right_t::control)};
        if (result == error_t::success)
            result =
                capability::install(root.cspace, 30U, object::reference(irq.object), owner_rights);
        const capability_id_t child = capability::encode_selector(0x5aU, 30U);
        if (result == error_t::success)
            result = capability::copy(delegated_cspace, child, root.cspace, 30U,
                                      capability::rights(capability::right_t::write));
        object::header_t* header = nullptr;
        if (result != error_t::success ||
            capability::lookup(delegated_cspace, child, object::type_t::interrupt,
                               capability::right_t::write, header) != error_t::success ||
            capability::lookup(delegated_cspace, child, object::type_t::interrupt,
                               capability::right_t::control, header) != error_t::denied)
            return error_t::invalid_argument;
        result =
            kernel::interrupt::bind(irq, object::reference(bootstrap::root_notification.object));
        // dispatch() itself no longer signals -- see its own comment: doing
        // so needs thread:: facilities (to wake a bound receiver) that
        // interrupt.hh cannot depend on without a circular include, so the
        // real caller (src/arch/arm64/arch.cc) finishes the signal after
        // dispatch() returns which notification/badge to use. This test
        // has no thread to wake, so it just finishes the signal itself,
        // the same plain notification::signal() dispatch() used to call
        // directly.
        const auto edge_dispatch = kernel::interrupt::dispatch(40U);
        if (edge_dispatch.delivered)
            notification::signal(*edge_dispatch.target, edge_dispatch.badge);
        if (result != error_t::success || !edge_dispatch.delivered ||
            notification::consume(bootstrap::root_notification) != (1ULL << 40U) ||
            kernel::interrupt::acknowledge(irq) != error_t::success ||
            kernel::interrupt::acknowledge(irq) != error_t::not_found)
            return error_t::invalid_argument;
        result = kernel::interrupt::bind(level_irq,
                                         object::reference(bootstrap::root_notification.object));
        const auto level_dispatch = kernel::interrupt::dispatch(41U);
        if (level_dispatch.delivered)
            notification::signal(*level_dispatch.target, level_dispatch.badge);
        if (result != error_t::success || !level_dispatch.delivered ||
            notification::consume(bootstrap::root_notification) != (1ULL << 41U) ||
            kernel::interrupt::acknowledge(level_irq) != error_t::success)
            return error_t::invalid_argument;
        /*
         * Storm containment, and then storm RECOVERY. Both halves matter:
         * `stormed` used to be a one-way latch, so a line that crossed the
         * threshold once stayed masked for the remaining uptime. A test
         * that only checked containment passed either way, which is how
         * that survived.
         *
         * Fixed ticks rather than the real clock so both the "still inside
         * the window" and "window has elapsed" cases are decidable.
         *
         * That choice is right for testing the state machine and it is also
         * this test's blind spot, so do NOT cite it as evidence that storm
         * containment works in production. Supplying one clock to both
         * record_delivery() and recover_stormed() is exactly what the real
         * call sites fail to do: deliveries stamp the window from whichever
         * CPU took the interrupt while the sweep always reads CPU 0, and
         * platform::timer::ticks() is per-CPU. The state machine below is
         * correct; the wiring around it is not. See checklist 0158.
         */
        constexpr u64 storm_at = 10U;
        for (u32 event = 0U; event <= kernel::interrupt::storm_threshold; ++event)
            (void)kernel::interrupt::record_delivery(irq, storm_at);
        if (!irq.stormed || !irq.masked || irq.suppressed < kernel::interrupt::storm_threshold ||
            kernel::interrupt::acknowledge(irq) != error_t::success)
            return error_t::invalid_argument;
        // Acknowledging does not clear the storm -- and cannot be what
        // does, since the mask stops any further delivery to acknowledge.
        if (!irq.stormed || !irq.masked)
            return error_t::invalid_argument;
        // A sweep still inside the detection window leaves it contained.
        kernel::interrupt::recover_stormed(storm_at + 1U);
        if (!irq.stormed || !irq.masked)
            return error_t::invalid_argument;
        // Once the window has elapsed, the timer-driven sweep re-arms the
        // line: storm containment is a rate limit, not a permanent kill.
        kernel::interrupt::recover_stormed(storm_at + kernel::interrupt::storm_window_ticks);
        if (irq.stormed || irq.masked)
            return error_t::invalid_argument;
        // And it is genuinely usable again, not merely flagged clean.
        if (!kernel::interrupt::record_delivery(
                irq, storm_at + kernel::interrupt::storm_window_ticks) ||
            kernel::interrupt::acknowledge(irq) != error_t::success)
            return error_t::invalid_argument;

        /*
         * Orphaned-line takeover, which is what makes a driver restartable
         * after it crashes while servicing its own interrupt.
         *
         * With a delivery outstanding, bind() must REFUSE while the owner is
         * still live -- rebinding under it would strand the acknowledge it
         * owes. But once that owner is gone, nothing will ever acknowledge,
         * so `active` would stay set and every future bind would fail with
         * busy for the remaining uptime. A destroyed notification is how
         * "the owner is gone" is detectable, so that is the discriminator.
         */
        if (!kernel::interrupt::record_delivery(
                irq, storm_at + 3U * kernel::interrupt::storm_window_ticks))
            return error_t::invalid_argument;
        if (kernel::interrupt::bind(irq, object::reference(
                                             bootstrap::root_notification.object)) != error_t::busy)
            return error_t::invalid_argument;
        // The owner's notification no longer resolves: its task was torn
        // down with the delivery in flight.
        irq.notification = {};
        if (kernel::interrupt::bind(irq, object::reference(
                                             bootstrap::root_notification.object)) !=
                error_t::success ||
            irq.active || irq.masked)
            return error_t::invalid_argument;
        const capability::derivation_id_t derivation =
            capability::slot_at(root.cspace, 30U).derivation;
        const u32 revoked = capability::revoke_descendants(derivation);
        const error_t lookup = capability::lookup(
            delegated_cspace, child, object::type_t::interrupt, capability::right_t::write, header);
        if (revoked != 1U || (lookup != error_t::denied && lookup != error_t::not_found))
            return error_t::invalid_argument;
        /*
         * Revocation must sever the AUTHORITY, not just the name. Before
         * this was enforced, the line stayed bound to the old owner's
         * notification and stayed unmasked, so a device kept firing into a
         * driver that no longer held a capability to it -- the delegation
         * was safe and the revocation was not.
         */
        if (!irq.masked || irq.active ||
            irq.notification.type != object::type_t::none)
            return error_t::invalid_argument;
        (void)capability::delete_capability(root.cspace, 30U);
        kernel::interrupt::unregister_irq(irq);
        kernel::interrupt::unregister_irq(level_irq);
        (void)object::unregister_object(object::reference(irq.object));
        (void)object::unregister_object(object::reference(level_irq.object));
        irq.object = {};
        level_irq.object = {};
        pr_info("[TEST] name=irq_ownership_delegation result=PASS irq=40 trigger=edge\n");
        pr_info("[TEST] name=irq_trigger_modes result=PASS edge=40 level=41 reserved=reject\n");
        pr_info("[TEST] name=irq_ack_deactivate result=PASS edge=2 level=1\n");
        pr_info("[TEST] name=irq_storm_containment result=PASS threshold=64 masked=1\n");
        pr_info("[TEST] name=irq_storm_recovery result=PASS window=100 rearmed=1\n");
        pr_info("[TEST] name=irq_orphan_takeover result=PASS live=busy orphaned=rebound\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::interrupt
