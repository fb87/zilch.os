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

    enum class memory_server_operation : word_t {
        allocate_frame = 1U,
        release_frame = 2U,
        query = 3U,
        shutdown = 4U,
        grant_frame = 5U,
    };

    inline constexpr word_t memory_server_max_handles = 12U;

    enum class memory_type : u8 {
        normal = 0U,
        device = 1U,
    };

    enum class memory_shareability : u8 {
        non_shareable = 0U,
        inner_shareable = 1U,
        outer_shareable = 2U,
    };

    [[nodiscard]] inline constexpr word_t
    encode_mapping_attributes(memory_type type, memory_shareability shareability) noexcept {
        return static_cast<word_t>(static_cast<u8>(type)) |
               (static_cast<word_t>(static_cast<u8>(shareability)) << 8U);
    }

    [[nodiscard]] inline constexpr memory_permission operator|(memory_permission left,
                                                               memory_permission right) noexcept {
        return static_cast<memory_permission>(static_cast<u8>(left) | static_cast<u8>(right));
    }

    [[nodiscard]] inline constexpr word_t encode(memory_permission permissions) noexcept {
        return static_cast<word_t>(static_cast<u8>(permissions));
    }
} // namespace sys::abi::v1
