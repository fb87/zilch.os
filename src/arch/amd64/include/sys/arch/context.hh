#pragma once

#include <sys/arch/v1/types.hh>

namespace sys::arch
{
    using user_context_t = v1::user_context_t;

    namespace context
    {
        inline void initialize_user(
            user_context_t& context,
            vaddr_t entry,
            vaddr_t stack,
            word_t argument) noexcept {
            context = {};
            context.general[0] = argument;
            context.stack_pointer = stack;
            context.instruction_pointer = entry;
            context.status = 0x202U;
        }
    } // namespace context
} // namespace sys::arch
