#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class memory_permission : u8 {
        none = 0U,
        read = 1U,
        write = 2U,
        execute = 4U,
    };

    [[nodiscard]] inline constexpr memory_permission operator|(memory_permission left,
                                                               memory_permission right) noexcept {
        return static_cast<memory_permission>(static_cast<u8>(left) | static_cast<u8>(right));
    }

    [[nodiscard]] inline constexpr word_t encode(memory_permission permissions) noexcept {
        return static_cast<word_t>(static_cast<u8>(permissions));
    }
} // namespace sys::abi::v1
