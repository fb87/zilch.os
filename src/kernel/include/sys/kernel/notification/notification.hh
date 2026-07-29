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
    };

    inline notification dynamic_notifications[dynamic_notification_count]{};

    inline void initialize(notification& value) noexcept {
        value.pending_badges = 0U;
    }

    inline void signal(notification& value, u64 badge) noexcept {
        __atomic_fetch_or(&value.pending_badges, badge, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline u64 consume(notification& value) noexcept {
        return __atomic_exchange_n(&value.pending_badges, 0U, __ATOMIC_ACQ_REL);
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
        capability::authority_guard authority_transaction{};
        object::header_t* header = nullptr;
        error_t result = capability::lookup(owner.cspace, selector, object::type_t::notification,
                                            capability::right_t::control, header);
        if (result != error_t::success)
            return result;
        auto& value = *reinterpret_cast<notification*>(header);
        if (&value < dynamic_notifications ||
            &value >= dynamic_notifications + dynamic_notification_count)
            return error_t::denied;
        const object::reference_t reference = object::reference(value.object);
        capability::revoke_reference_locked(reference);
        authority_transaction.release();
        result = object::unregister_object(reference);
        if (result != error_t::success)
            return result;
        value.object = {};
        initialize(value);
        __atomic_store_n(&value.allocated, 0U, __ATOMIC_RELEASE);
        return error_t::success;
    }

    [[nodiscard]] inline bool database_valid() noexcept {
        for (const notification& value : dynamic_notifications) {
            const bool allocated = __atomic_load_n(&value.allocated, __ATOMIC_ACQUIRE) != 0U;
            if (allocated) {
                if (value.object.type != object::type_t::notification)
                    return false;
            } else if (value.object.type != object::type_t::none ||
                       __atomic_load_n(&value.pending_badges, __ATOMIC_ACQUIRE) != 0U) {
                return false;
            }
        }
        return true;
    }

} // namespace sys::kernel::notification
