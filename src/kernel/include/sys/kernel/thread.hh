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

    using kernel_step_t = void (*)(void*) noexcept;

    struct thread_t {
        object::header_t header{};
        thread_id_t id{};
        space_id_t address_space{};
        state_t state{state_t::inactive};
        u8 priority{};
        cpu_id_t cpu{};
        u32 quantum_ticks{1U};
        u32 remaining_ticks{1U};
        kernel_step_t kernel_step{};
        void* kernel_argument{};
        arch::user_context_t context{};
    };

    inline void initialize(
        thread_t& target,
        thread_id_t id,
        space_id_t address_space,
        vaddr_t entry,
        vaddr_t stack) noexcept
    {
        target.id = id;
        target.address_space = 0U;
        target.state = state_t::inactive;
        target.priority = 0U;
        target.cpu = 0U;
        target.quantum_ticks = 1U;
        target.remaining_ticks = 1U;
        target.kernel_step = nullptr;
        target.kernel_argument = nullptr;
        target.id = id;
        target.address_space = address_space;
        target.state = state_t::ready;
        arch::context::initialize_user(target.context, entry, stack, 0U);
    }

    inline void initialize_kernel(
        thread_t& target,
        thread_id_t id,
        cpu_id_t cpu,
        kernel_step_t step,
        void* argument,
        u32 quantum_ticks = 1U) noexcept
    {
        target.id = id;
        target.address_space = 0U;
        target.state = state_t::inactive;
        target.priority = 0U;
        target.cpu = cpu;
        target.quantum_ticks = 1U;
        target.remaining_ticks = 1U;
        target.kernel_step = nullptr;
        target.kernel_argument = nullptr;
        target.id = id;
        target.cpu = cpu;
        target.state = state_t::ready;
        target.kernel_step = step;
        target.kernel_argument = argument;
        target.quantum_ticks = quantum_ticks == 0U ? 1U : quantum_ticks;
        target.remaining_ticks = target.quantum_ticks;
    }
} // namespace sys::kernel::thread
