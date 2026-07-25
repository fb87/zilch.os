#pragma once

#include <sys/kernel/object.hh>
#include <sys/platform/platform.hh>

namespace sys::kernel::interrupt
{
    struct interrupt_t {
        object::header_t header;
        irq_id_t irq;
        thread_id_t bound_thread;
    };

    inline void acknowledge_and_complete() noexcept {
        const irq_id_t irq = platform::interrupt::acknowledge();
        platform::interrupt::complete(irq);
    }
} // namespace sys::kernel::interrupt
