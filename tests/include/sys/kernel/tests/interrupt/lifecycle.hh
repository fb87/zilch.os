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
        kernel::interrupt::initialize(irq, 40U, kernel::interrupt::trigger::edge);
        error_t result = object::register_dynamic_object(irq.object, object::type_t::interrupt);
        if (result == error_t::success)
            result = kernel::interrupt::register_irq(irq);
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
        if (result != error_t::success || !kernel::interrupt::dispatch(40U) ||
            notification::consume(bootstrap::root_notification) != (1ULL << 40U) ||
            kernel::interrupt::acknowledge(irq) != error_t::success ||
            kernel::interrupt::acknowledge(irq) != error_t::not_found)
            return error_t::invalid_argument;
        for (u32 event = 0U; event <= kernel::interrupt::storm_threshold; ++event)
            (void)kernel::interrupt::record_delivery(irq, 10U);
        if (!irq.stormed || !irq.masked || irq.suppressed < kernel::interrupt::storm_threshold ||
            kernel::interrupt::acknowledge(irq) != error_t::success)
            return error_t::invalid_argument;
        const capability::derivation_id_t derivation =
            capability::slot_at(root.cspace, 30U).derivation;
        const u32 revoked = capability::revoke_descendants(derivation);
        const error_t lookup = capability::lookup(
            delegated_cspace, child, object::type_t::interrupt, capability::right_t::write, header);
        if (revoked != 1U || (lookup != error_t::denied && lookup != error_t::not_found))
            return error_t::invalid_argument;
        (void)capability::delete_capability(root.cspace, 30U);
        kernel::interrupt::unregister_irq(irq);
        (void)object::unregister_object(object::reference(irq.object));
        irq.object = {};
        pr_info("[TEST] name=irq_ownership_delegation result=PASS irq=40 trigger=edge\n");
        pr_info("[TEST] name=irq_ack_deactivate result=PASS delivered=2 acknowledged=2\n");
        pr_info("[TEST] name=irq_storm_containment result=PASS threshold=64 masked=1\n");
        return error_t::success;
    }
} // namespace sys::kernel::tests::interrupt
