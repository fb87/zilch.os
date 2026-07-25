#pragma once

#include <sys/arch/irq.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/entry.hh>
#include <sys/kernel/printk.hh>
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
                            static_cast<word_t>(initial_fuzz_seed(id)));
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

    [[nodiscard]] inline u32 next_runnable(u32 after) noexcept
    {
        for (u32 offset = 1U; offset <= user_thread_count; ++offset) {
            const u32 candidate = (after + offset) % user_thread_count;
            if (runnable(user_threads[candidate])) {
                return candidate;
            }
        }
        return after;
    }

    inline void load_user(arch::thread::context& frame, u32 id) noexcept
    {
        user_threads[current_user_thread].current_state =
            user_threads[current_user_thread].current_state == state::running
                ? state::ready : user_threads[current_user_thread].current_state;
        current_user_thread = id;
        user_threads[id].current_state = state::running;
        user_threads[id].address_space.activate();
        arch::thread::copy(frame, user_threads[id].context);
    }

    inline void schedule_user(arch::thread::context& frame) noexcept
    {
        if (!user_execution_active || arch::cpu::current_id() != 0U) {
            return;
        }
        save_current_user(frame);
        const u32 next = next_runnable(current_user_thread);
        if (next != current_user_thread || runnable(user_threads[next])) {
            load_user(frame, next);
        }
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

    [[nodiscard]] inline bool handle_user_fault(arch::thread::context& frame,
                                                u64 vector,
                                                u64 syndrome,
                                                vaddr_t fault_address) noexcept
    {
        if (!user_execution_active || vector != 8U) {
            return false;
        }
        const u64 exception_class = (syndrome >> 26U) & 0x3fU;
        if (exception_class != 0x20U && exception_class != 0x21U
            && exception_class != 0x24U && exception_class != 0x25U) {
            return false;
        }

        thread& current = user_threads[current_user_thread];
        ++current.faults;
        current.current_state = state::faulted;
        pr_warn("user fault contained thread=%llu esr=%llx far=%llx pc=%llx\n",
                static_cast<unsigned long long>(current.id),
                static_cast<unsigned long long>(syndrome),
                static_cast<unsigned long long>(fault_address),
                static_cast<unsigned long long>(frame.instruction_pointer));
        const u32 next = next_runnable(current_user_thread);
        if (next == current_user_thread && !runnable(user_threads[next])) {
            pr_err("user fault: no runnable threads remain\n");
            return false;
        }
        load_user(frame, next);
        return true;
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
