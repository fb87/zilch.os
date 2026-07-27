#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class fault_kind : u8 {
        none,
        instruction_abort,
        data_abort,
        alignment,
        invalid_context,
    };

    enum class fault_disposition : u8 {
        pending,
        resume,
        terminate,
    };

    struct fault_message final {
        word_t kind{};
        word_t syndrome{};
        word_t address{};
        word_t instruction_pointer{};
    };
} // namespace sys::abi::v1
