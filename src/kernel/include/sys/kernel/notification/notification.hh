#pragma once

#include <sys/kernel/capability/cspace.hh>
#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/task/task.hh>
#include <sys/types.hh>

namespace sys::kernel::notification
{
    inline constexpr u32 dynamic_notification_count = 16U;
    struct notification {
        object::header_t object{};
        volatile u64 pending_badges{};
        volatile u32 allocated{};
        /*
         * The thread a blocked ipc_receive() should also wake for, set by
         * notification_bind (see thread::signal_notification_locked() in
         * scheduler.hh, which is the wake-aware caller of signal() below --
         * this file has no thread:: facilities and stays that way).
         * Generation-checked like every other cross-object reference in
         * this kernel: a stale bind from a torn-down thread simply fails to
         * resolve later rather than needing to be found and cleared from
         * here. What DOES need explicit clearing is this field itself, on
         * reuse -- see initialize() -- because a fresh notification at a
         * recycled slot must not inherit whichever thread the previous
         * occupant happened to have bound.
         */
        object::reference_t bound_thread{};
    };

    inline notification dynamic_notifications[dynamic_notification_count]{};

    inline void initialize(notification& value) noexcept {
        value.pending_badges = 0U;
        value.bound_thread = {};
    }

    inline void signal(notification& value, u64 badge) noexcept {
        __atomic_fetch_or(&value.pending_badges, badge, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline u64 consume(notification& value) noexcept {
        return __atomic_exchange_n(&value.pending_badges, 0U, __ATOMIC_ACQ_REL);
    }

    /*
     * Binds `thread_reference` (always the calling thread's own object --
     * callers must never pass another thread's reference; see
     * notification_bind's dispatch in control.hh) to this notification, so
     * a future signal() can wake it out of a blocked ipc_receive(). Same
     * shape as interrupt::bind() in interrupt.hh: resolve, validate type,
     * store. One waiter per notification -- confirmed sufficient for every
     * real usage in this codebase (serial-driver, virtio-driver,
     * domain-manager's IRQ aggregation, root's own readiness notification
     * are all many-signalers/one-consumer already) -- rebinding an
     * already-bound notification fails closed rather than silently
     * replacing the existing waiter.
     */
    [[nodiscard]] inline error_t bind(notification& value,
                                      const object::reference_t& thread_reference) noexcept {
        object::header_t* header = object::resolve(thread_reference);
        if (header == nullptr || header->type != object::type_t::thread)
            return error_t::invalid_argument;
        if (value.bound_thread.type != object::type_t::none)
            return error_t::busy;
        value.bound_thread = thread_reference;
        return error_t::success;
    }

    inline void unbind(notification& value) noexcept {
        value.bound_thread = {};
    }

    [[nodiscard]] inline error_t create(task::task& owner, capability_id_t selector) noexcept {
        if (selector >= capability::cspace_slot_count)
            return error_t::invalid_argument;
        if (capability::slot_at(owner.cspace, selector).object.type != object::type_t::none)
            return error_t::busy;
        for (u32 index = 0U; index < dynamic_notification_count; ++index) {
            notification& value = dynamic_notifications[index];
            u32 expected = 0U;
            if (!__atomic_compare_exchange_n(&value.allocated, &expected, 1U, false,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                continue;
            initialize(value);
            error_t result =
                object::register_dynamic_object(value.object, object::type_t::notification);
            if (result == error_t::success) {
                result =
                    capability::install(owner.cspace, selector, object::reference(value.object),
                                        {static_cast<u32>(capability::right_t::read) |
                                         static_cast<u32>(capability::right_t::write) |
                                         static_cast<u32>(capability::right_t::grant) |
                                         static_cast<u32>(capability::right_t::control)});
            }
            if (result != error_t::success) {
                if (value.object.type != object::type_t::none)
                    (void)object::unregister_object(object::reference(value.object));
                value.object = {};
                __atomic_store_n(&value.allocated, 0U, __ATOMIC_RELEASE);
            }
            return result;
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t destroy(task::task& owner, capability_id_t selector) noexcept {
        notification* target = nullptr;
        object::reference_t reference{};
        {
            capability::authority_guard authority_transaction{};
            object::header_t* header = nullptr;
            const error_t looked_up = capability::lookup(
                owner.cspace, selector, object::type_t::notification,
                capability::right_t::control, header);
            if (looked_up != error_t::success)
                return looked_up;
            auto& value = *reinterpret_cast<notification*>(header);
            if (&value < dynamic_notifications ||
                &value >= dynamic_notifications + dynamic_notification_count)
                return error_t::denied;
            target = &value;
            reference = object::reference(value.object);
        }

        /*
         * Unregister BEFORE revoking the capability, not after.
         *
         * The other order leaks the notification outright whenever
         * unregister fails, which it genuinely can -- it returns busy when
         * another destroyer wins the table exchange. The capability was
         * already gone by then, so nothing could name the object to try
         * again, while the object stayed registered and `allocated`: a
         * dynamic pool slot lost for the lifetime of the boot. Found by
         * arming the teardown injection site (TST-024) at the first attempt.
         *
         * Reversed, a failed unregister changes nothing at all and the
         * caller's retry works. Unregistering first is safe because the
         * object table is generation-checked: a capability naming a
         * departed object fails closed on use, so the window before the
         * revoke grants no authority. And the revoke only scans cspaces
         * comparing references -- it never resolves the object -- so it
         * does not care that the object is already gone.
         *
         * Split into two authority transactions rather than one because
         * unregister_object() calls synchronize_readers() and takes the
         * table lock; the original code released the authority lock before
         * it for exactly that reason, and the ordering constraint has not
         * changed.
         */
        const error_t result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;

        {
            capability::authority_guard authority_transaction{};
            capability::revoke_reference_locked(reference);
        }

        target->object = {};
        initialize(*target);
        __atomic_store_n(&target->allocated, 0U, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline bool database_valid() noexcept {
        for (const notification& value : dynamic_notifications) {
            const bool allocated = __atomic_load_n(&value.allocated, __ATOMIC_ACQUIRE) != 0U;
            if (allocated) {
                if (value.object.type != object::type_t::notification)
                    return false;
            } else if (value.object.type != object::type_t::none ||
                       __atomic_load_n(&value.pending_badges, __ATOMIC_ACQUIRE) != 0U ||
                       value.bound_thread.type != object::type_t::none) {
                return false;
            }
        }
        return true;
    }

} // namespace sys::kernel::notification
