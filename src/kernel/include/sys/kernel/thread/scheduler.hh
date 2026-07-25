#pragma once

#include <sys/arch/irq.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/thread/thread.hh>
#include <sys/types.hh>

namespace sys::kernel::thread
{
    inline constexpr u32 user_thread_count = 10U;
    inline thread user_threads[user_thread_count]{};
    inline u32 current_user_thread = 0U;
    inline bool user_execution_active = false;

    inline void initialize_user_threads() noexcept
    {
        for (u32 id = 0U; id < user_thread_count; ++id) {
            initialize_user(user_threads[id], static_cast<thread_id_t>(id),
                            static_cast<word_t>(id),
                            static_cast<word_t>((id + 1U) * 20000U));
        }
        current_user_thread = 0U;
        user_execution_active = false;
    }

    inline void save_current_user(const arch::thread::context& frame) noexcept
    {
        if (user_execution_active) {
            arch::thread::copy(user_threads[current_user_thread].context, frame);
        }
    }

    inline void load_user(arch::thread::context& frame, u32 id) noexcept
    {
        current_user_thread = id;
        user_threads[id].address_space.activate();
        arch::thread::copy(frame, user_threads[id].context);
    }

    inline void schedule_user(arch::thread::context& frame) noexcept
    {
        if (!user_execution_active || arch::cpu::current_id() != 0U) {
            return;
        }
        save_current_user(frame);
        load_user(frame, (current_user_thread + 1U) % user_thread_count);
    }

    [[nodiscard]] inline bool handle_user_syscall(arch::thread::context& frame,
                                                  u64 vector,
                                                  u64 syndrome) noexcept
    {
        if (!user_execution_active) {
            return false;
        }
        return syscall::dispatch_ipc(user_threads[current_user_thread],
                                     frame, vector, syndrome);
    }

    [[noreturn]] inline void enter_first_user_thread() noexcept
    {
        arch::irq::disable();
        current_user_thread = 0U;
        user_execution_active = true;
        user_threads[0].current_state = state::running;
        user_threads[0].address_space.activate();
        arch::thread::enter_user(user_threads[0].context);
    }
} // namespace sys::kernel::thread
