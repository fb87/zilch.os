#pragma once

#include <sys/kernel/boot/bootinfo.hh>
#include <sys/kernel/hypervisor.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/notification/notification.hh>
#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::bootstrap
{
    inline notification::notification root_notification{};
    inline interrupt::interrupt_t root_timer_interrupt{};

    [[nodiscard]] inline error_t initialize_objects() noexcept {
        error_t result = memory::initialize_objects();
        if (result != error_t::success)
            return result;
        notification::initialize(root_notification);
        result =
            object::register_object(root_notification.object, object::bootstrap_id::notification,
                                    object::type_t::notification);
        if (result != error_t::success)
            return result;
        root_timer_interrupt.irq = 27U;
        root_timer_interrupt.notification = object::reference(root_notification.object);
        result = object::register_object(root_timer_interrupt.object,
                                         object::bootstrap_id::timer_interrupt,
                                         object::type_t::interrupt);
        if (result != error_t::success)
            return result;
        result = hypervisor::initialize();
        if (result == error_t::unsupported)
            return error_t::success;
        return result;
    }
} // namespace sys::kernel::bootstrap
