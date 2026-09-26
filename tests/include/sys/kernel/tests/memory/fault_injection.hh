#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/ipc/endpoint.hh>
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

        /*
         * Teardown injection (TST-024), which the creation sites above do
         * not reach.
         *
         * Teardown is the half nobody writes a recovery path for: creation
         * failing is expected and handled, destruction failing is usually
         * assumed away, and the object table's unregister genuinely can
         * return busy when a concurrent destroyer wins the exchange. What
         * must hold is that a failed teardown leaves the object still
         * whole -- still registered, still accounted, still destroyable --
         * rather than half-removed and unreachable, which is a permanent
         * leak of a bounded pool slot that nothing can ever reclaim.
         */
        const u64 live_baseline = object::accounting.live[type_index];
        const u32 free_baseline = memory::free_pages;

        if (notification::create(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;

        verification::configure_failure(injection_site::object_unregistration, 1U);
        const error_t torn = notification::destroy(root, probe_selector);
        verification::configure_failure(injection_site::object_unregistration, 0U);

        if (torn == error_t::success)
            return error_t::invalid_argument; // injection did not take
        if (object::accounting.live[type_index] != live_baseline + 1U)
            return error_t::invalid_argument; // object lost while destruction failed

        /*
         * The retry is the whole point. An object whose failed teardown
         * left it unreachable would fail here, and that is the difference
         * between a transient error and a leaked pool slot.
         */
        if (notification::destroy(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;
        if (object::accounting.live[type_index] != live_baseline)
            return error_t::invalid_argument;
        if (capability::slot_at(root.cspace, probe_selector).object.type != object::type_t::none)
            return error_t::invalid_argument;
        if (memory::free_pages != free_baseline)
            return error_t::invalid_argument;

        /*
         * The endpoint path had the same defect and one more on top: it
         * latches `retiring` before tearing down, so the old order left a
         * failed teardown with the capability revoked AND the flag set --
         * an endpoint that could not be named, and would have answered
         * `busy` forever to the retry even if it could. Worth its own case
         * rather than trusting the notification result to generalise,
         * because the compensating rollback of that flag is unique to it.
         */
        const auto endpoint_index = static_cast<u32>(object::type_t::endpoint);
        const u64 endpoints_baseline = object::accounting.live[endpoint_index];

        if (::sys::kernel::ipc::create(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;

        verification::configure_failure(injection_site::object_unregistration, 1U);
        const error_t endpoint_torn = ::sys::kernel::ipc::destroy(root, probe_selector);
        verification::configure_failure(injection_site::object_unregistration, 0U);

        if (endpoint_torn == error_t::success)
            return error_t::invalid_argument; // injection did not take
        if (object::accounting.live[endpoint_index] != endpoints_baseline + 1U)
            return error_t::invalid_argument;

        // Must recover completely: a latched `retiring` would surface here
        // as busy rather than success.
        if (::sys::kernel::ipc::destroy(root, probe_selector) != error_t::success)
            return error_t::invalid_argument;
        if (object::accounting.live[endpoint_index] != endpoints_baseline)
            return error_t::invalid_argument;
        if (capability::slot_at(root.cspace, probe_selector).object.type != object::type_t::none)
            return error_t::invalid_argument;

        pr_info("[TEST] name=fault_injection_rollback result=PASS sites=%u targeted=1 "
                "no_object_leak=1 no_capability_leak=1 no_page_leak=1 recovers=1 "
                "teardown_injected=2 teardown_retry_converges=2\n",
                injected);
        return error_t::success;
    }
} // namespace sys::kernel::tests::fault_injection
