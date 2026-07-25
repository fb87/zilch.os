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
    };

    inline void initialize_user(thread& value, thread_id_t id,
                                word_t argument0, word_t argument1) noexcept
    {
        value.id = id;
        value.current_state = state::ready;
        value.address_space.initialize();
        arch::thread::initialize_user(value.context,
                                      arch::space::entry(),
                                      arch::space::stack_top(),
                                      argument0,
                                      argument1);
    }
} // namespace sys::kernel::thread
