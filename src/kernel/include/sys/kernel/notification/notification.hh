#pragma once

#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::notification
{
    struct notification {
        object::header_t object{};
        volatile u64 pending_badges{};
        object::reference_t waiter{};
    };

    inline void initialize(notification& value) noexcept {
        value.pending_badges = 0U;
        value.waiter = {};
    }

    inline void signal(notification& value, u64 badge) noexcept {
        __atomic_fetch_or(&value.pending_badges, badge, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline u64 consume(notification& value) noexcept {
        return __atomic_exchange_n(&value.pending_badges, 0U, __ATOMIC_ACQ_REL);
    }
} // namespace sys::kernel::notification
