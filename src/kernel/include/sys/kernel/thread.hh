#pragma once

#include <sys/arch/arch.hh>
#include <sys/kernel/object.hh>

namespace sys::kernel::thread
{
    enum class state_t : u8 {
        inactive,
        ready,
        running,
        blocked_ipc,
        blocked_fault,
        suspended,
        dead,
    };

    struct thread_t {
        object::header_t header;
        thread_id_t id;
        space_id_t address_space;
        state_t state;
        u8 priority;
        arch::user_context_t context;
    };

    inline void initialize(
        thread_t& thread,
        thread_id_t id,
        space_id_t address_space,
        vaddr_t entry,
        vaddr_t stack) noexcept {
        thread.id = id;
        thread.address_space = address_space;
        thread.state = state_t::ready;
        thread.priority = 0U;
        arch::context::initialize_user(thread.context, entry, stack, 0U);
    }
} // namespace sys::kernel::thread
