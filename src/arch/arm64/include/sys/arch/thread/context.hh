#pragma once

#include <sys/arch/exception.hh>
#include <sys/arch/space/address_space.hh>
#include <sys/types.hh>

namespace sys::arch::thread
{
    using context = exception::frame_t;

    inline void clear(context& value) noexcept {
        for (usize_t index = 0U; index < 31U; ++index) {
            value.x[index] = 0U;
        }
        value.vector = 0U;
        value.stack_pointer = 0U;
        value.instruction_pointer = 0U;
        value.status = 0U;
    }

    inline void copy(context& destination, const context& source) noexcept {
        for (usize_t index = 0U; index < 31U; ++index) {
            destination.x[index] = source.x[index];
        }
        destination.vector = source.vector;
        destination.stack_pointer = source.stack_pointer;
        destination.instruction_pointer = source.instruction_pointer;
        destination.status = source.status;
    }

    [[nodiscard]] inline bool valid_user(const context& value) noexcept {
        constexpr vaddr_t user_code_begin = arch::space::user_code;
        const vaddr_t user_code_end = arch::space::user_image_end();
        constexpr vaddr_t user_stack_begin = arch::space::user_stack_base;
        constexpr vaddr_t user_stack_end = arch::space::stack_top();

        return value.instruction_pointer >= user_code_begin &&
               value.instruction_pointer < user_code_end &&
               (value.instruction_pointer & 0x3U) == 0U &&
               value.stack_pointer >= user_stack_begin && value.stack_pointer <= user_stack_end &&
               (value.stack_pointer & 0xfU) == 0U && (value.status & 0xfU) == 0U;
    }

    inline void initialize_user(context& value, vaddr_t entry, vaddr_t stack, word_t argument0,
                                word_t argument1) noexcept {
        clear(value);
        value.x[0] = argument0;
        value.x[1] = argument1;
        value.stack_pointer = stack;
        value.instruction_pointer = entry;
        value.status = 0U; // EL0t
    }

    /* Publishes a completed IPC's status/error code into the register a
     * resumed thread reads its syscall result from. */
    inline void set_ipc_result(context& value, word_t result) noexcept {
        value.x[0] = result;
    }

    /* Publishes a completed IPC's sender/badge and four-word message payload
     * into the registers a resumed thread reads them from. */
    inline void set_ipc_message(context& value, word_t sender_or_badge,
                                const word_t message[4]) noexcept {
        value.x[1] = sender_or_badge;
        for (usize_t index = 0U; index < 4U; ++index)
            value.x[index + 2U] = message[index];
    }
} // namespace sys::arch::thread
