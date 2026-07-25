#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    inline constexpr usize_t message_register_count = 8U;

    struct MessageTag final
    {
        word_t raw;
    };

    struct Message final
    {
        MessageTag tag;
        word_t words[message_register_count];
    };
} // namespace sys::abi::v1
