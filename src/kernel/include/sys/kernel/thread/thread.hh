#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/arch/thread/context.hh>
#include <sys/kernel/space/address_space.hh>
#include <sys/types.hh>

namespace sys::kernel::thread
{
    enum class state : u8
    {
        inactive,
        ready,
        running,
        blocked,
        faulted,
        terminated,
    };

    struct thread
    {
        thread_id_t id{};
        state current_state{state::inactive};
        space::address_space address_space{};
        arch::thread::context context{};
        u64 reports{};
        u64 fuzz_seed{};
        u64 fuzz_iterations{};
        u64 fuzz_failures{};
        u64 faults{};
    };

    [[nodiscard]] inline constexpr u64 initial_fuzz_seed(thread_id_t id) noexcept
    {
        return 0x9e3779b97f4a7c15ULL
            ^ (static_cast<u64>(id) * 0xbf58476d1ce4e5b9ULL);
    }

    inline void initialize_user(thread& value, thread_id_t id,
                                word_t argument0, word_t argument1) noexcept
    {
        value.id = id;
        value.current_state = state::ready;
        value.reports = 0U;
        value.fuzz_seed = static_cast<u64>(argument1);
        value.fuzz_iterations = 0U;
        value.fuzz_failures = 0U;
        value.faults = 0U;
        value.address_space.initialize();
        arch::thread::initialize_user(value.context,
                                      arch::space::entry(),
                                      arch::space::stack_top(),
                                      argument0,
                                      argument1);
    }

    [[nodiscard]] inline bool runnable(const thread& value) noexcept
    {
        return value.current_state == state::ready
            || value.current_state == state::running;
    }

    [[nodiscard]] inline bool validate(const thread& value) noexcept
    {
        if (value.current_state == state::running
            || value.current_state == state::ready) {
            return value.context.instruction_pointer != 0U
                && value.context.stack_pointer != 0U;
        }
        return true;
    }
} // namespace sys::kernel::thread
