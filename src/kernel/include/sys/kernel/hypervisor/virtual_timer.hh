#pragma once

#include <sys/types.hh>

namespace sys::kernel::hypervisor
{
    struct virtual_timer_state {
        u64 deadline{};
        bool armed{};

        void reset() noexcept {
            deadline = 0U;
            armed = false;
        }
    };
} // namespace sys::kernel::hypervisor
