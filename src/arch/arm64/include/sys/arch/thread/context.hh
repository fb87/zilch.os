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

    [[nodiscard]] inline bool valid_user(const context& value) noexcept
    {
        constexpr vaddr_t user_code_begin = 0x10000000ULL;
        constexpr vaddr_t user_code_end = 0x10001000ULL;
        constexpr vaddr_t user_stack_begin = 0x10010000ULL;
        constexpr vaddr_t user_stack_end = 0x10011000ULL;

        return value.instruction_pointer >= user_code_begin
            && value.instruction_pointer < user_code_end
            && (value.instruction_pointer & 0x3U) == 0U
            && value.stack_pointer >= user_stack_begin
            && value.stack_pointer <= user_stack_end
            && (value.stack_pointer & 0xfU) == 0U
            && (value.status & 0xfU) == 0U;
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
