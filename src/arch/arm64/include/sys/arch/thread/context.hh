#pragma once

#include <sys/arch/exception.hh>
#include <sys/types.hh>

namespace sys::arch::thread
{
    using context = exception::frame_t;

    inline void clear(context& value) noexcept
    {
        for (usize_t index = 0U; index < 31U; ++index) {
            value.x[index] = 0U;
        }
        value.vector = 0U;
        value.stack_pointer = 0U;
        value.instruction_pointer = 0U;
        value.status = 0U;
    }

    inline void copy(context& destination, const context& source) noexcept
    {
        for (usize_t index = 0U; index < 31U; ++index) {
            destination.x[index] = source.x[index];
        }
        destination.vector = source.vector;
        destination.stack_pointer = source.stack_pointer;
        destination.instruction_pointer = source.instruction_pointer;
        destination.status = source.status;
    }

    inline void initialize_user(context& value, vaddr_t entry, vaddr_t stack,
                                word_t argument0, word_t argument1) noexcept
    {
        clear(value);
        value.x[0] = argument0;
        value.x[1] = argument1;
        value.stack_pointer = stack;
        value.instruction_pointer = entry;
        value.status = 0U; // EL0t
    }
} // namespace sys::arch::thread
