#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/kernel/emergency.hh>
#include <sys/types.hh>

namespace sys::kernel::panic
{
    enum class reason : u32 {
        fatal_exception = 1U,
        stack_corruption = 2U,
        internal_failure = 3U,
    };

    inline void capture(reason cause, u32 level, u64 vector, u64 syndrome, u64 fault_address,
                        u64 instruction_pointer) noexcept {
        const emergency::event kind = cause == reason::stack_corruption
                                          ? emergency::event::stack_corruption
                                          : emergency::event::fatal_exception;
        emergency::append(kind, static_cast<u64>(cause), level, vector, syndrome,
                          instruction_pointer);
        emergency::preserve(level, vector, syndrome, fault_address, instruction_pointer);
    }

    [[noreturn]] inline void stop(reason cause, u32 level, u64 vector, u64 syndrome,
                                  u64 fault_address, u64 instruction_pointer) noexcept {
        arch::irq::mask_all();
        capture(cause, level, vector, syndrome, fault_address, instruction_pointer);
        arch::cpu::halt_barrier();
        arch::cpu::halt();
    }
} // namespace sys::kernel::panic
