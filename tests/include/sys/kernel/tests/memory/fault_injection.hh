#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>
#include <sys/kernel/verification/hooks.hh>

namespace sys::kernel::tests::fault_injection
{
    /*
     * Systematic rollback under injected failure (TST-024).
     *
     * A composite operation that fails partway must leave nothing behind.
     * The failure paths themselves are easy to write and almost never
     * executed, which is exactly the combination that rots: the common
     * case keeps working while the cleanup quietly stops matching what the
     * success path allocates.
     *
     * `notification::create()` is the vehicle because it is the smallest
     * operation that does both halves -- register a dynamic object, then
     * install a capability naming it -- so a failure at either point has
     * something real to undo. Each site is driven in turn and the same
     * three invariants are checked every time: the object table's live
     * count for the type, the target CSpace slot, and the free-page count.
     *
     * The signature is taken before and after rather than merely checking
     * the call returned an error. An operation that fails loudly while
     * leaking a registered object still "fails", and would pass a weaker
     * test while exhausting a bounded pool over time.
     */
    [[nodiscard]] inline error_t run(task::task& root) noexcept
    {
        using verification::injection_site;
        constexpr capability_id_t probe_selector = 19U;
        const auto type_index = static_cast<u32>(object::type_t::notification);

        // A slot that must start free, or the test proves nothing.
        if (capability::slot_at(root.cspace, probe_selector).object.type != object::type_t::none)
            return error_t::invalid_argument;

        const injection_site sites[] = {
            injection_site::object_registration,
            injection_site::capability_install,
            injection_site::page_allocation,
        };

        u32 injected = 0U;
        for (const injection_site site : sites) {
            const u64 live_before = object::accounting.live[type_index];
            const u32 free_before = memory::free_pages;

            verification::configure_failure(site, 1U);
            const error_t result = notification::create(root, probe_selector);
            verification::configure_failure(site, 0U); // disarm whatever went unused

            /*
             * page_allocation is deliberately allowed to succeed: notification
             * creation does not allocate a page, so arming that site proves
             * the injection is targeted rather than global -- an injector that
             * failed everything would make the other two cases meaningless.
             */
            if (site == injection_site::page_allocation) {
                if (result != error_t::success)
                    return error_t::invalid_argument;
                if (notification::destroy(root, probe_selector) != error_t::success)
                    return error_t::invalid_argument;
            } else {
                ++injected;
                if (result == error_t::success)
                    return error_t::invalid_argument; // injection did not take
                if (object::accounting.live[type_index] != live_before)
                    return error_t::invalid_argument; // leaked a registered object
                if (capability::slot_at(root.cspace, probe_selector).object.type !=
                    object::type_t::none)
                    return error_t::invalid_argument; // left a capability behind
            }

            if (memory::free_pages != free_before)
                return error_t::invalid_argument; // leaked or over-freed a page
        }

        if (injected != 2U)
            return error_t::invalid_argument;

        /*
         * And the operation must still work once nothing is armed. A
         * rollback that quietly corrupted shared state would show up here
         * rather than above.
         */
        if (notification::create(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;
        if (notification::destroy(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;
        if (capability::slot_at(root.cspace, probe_selector).object.type != object::type_t::none)
            return error_t::invalid_argument;

        pr_info("[TEST] name=fault_injection_rollback result=PASS sites=%u targeted=1 "
                "no_object_leak=1 no_capability_leak=1 no_page_leak=1 recovers=1\n",
                injected);
        return error_t::success;
    }
} // namespace sys::kernel::tests::fault_injection
