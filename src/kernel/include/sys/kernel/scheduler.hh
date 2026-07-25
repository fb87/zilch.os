#pragma once

#include <sys/kernel/thread.hh>

namespace sys::kernel::scheduler
{
    inline constexpr usize_t priority_count = 256U;

    struct run_queue_t {
        thread::thread_t* current;
        usize_t ready_count;
    };

    inline run_queue_t boot_run_queue{};

    inline void initialize() noexcept {
        boot_run_queue = {};
    }

    inline void make_ready(thread::thread_t& thread) noexcept {
        thread.state = thread::state_t::ready;
        ++boot_run_queue.ready_count;
    }
} // namespace sys::kernel::scheduler
