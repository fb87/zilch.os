#pragma once

#include <sys/kernel/object.hh>
#include <sys/kernel/object/table.hh>
#include <sys/platform/platform.hh>

namespace sys::kernel::interrupt
{
    struct interrupt_t {
        object::header_t object{};
        irq_id_t irq{};
        object::reference_t notification{};
        bool masked{true};
    };

    inline void bind(interrupt_t& value, const object::reference_t& notification) noexcept {
        value.notification = notification;
    }

    inline void acknowledge_and_complete() noexcept {
        const irq_id_t irq = platform::interrupt::acknowledge();
        platform::interrupt::complete(irq);
    }
} // namespace sys::kernel::interrupt
