#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class syscall : word_t {
        ipc = 0U,
        control = 1U,
    };

    enum class ipc_operation : word_t {
        call = 0U,
        receive = 1U,
        reply_receive = 2U,
        cancel = 3U,
        reply = 4U,
    };

    inline constexpr word_t capability_transfer_valid = 1ULL << 63U;
    inline constexpr word_t ipc_timeout_valid = 1ULL << 63U;
    inline constexpr u64 no_timeout = 0U;

    [[nodiscard]] inline constexpr word_t encode_timeout(u64 ticks) noexcept {
        return ipc_timeout_valid | (ticks & ~ipc_timeout_valid);
    }

    [[nodiscard]] inline constexpr word_t encode_capability_transfer(capability_id_t source,
                                                                     capability_id_t destination,
                                                                     u32 rights,
                                                                     u32 badge) noexcept {
        return capability_transfer_valid | (source & 0x3fU) | ((destination & 0x3fU) << 6U) |
               ((static_cast<word_t>(rights) & 0x3fU) << 12U) | (static_cast<word_t>(badge) << 32U);
    }

    inline constexpr capability_id_t ipc_server_a = 10U;
    inline constexpr capability_id_t ipc_server_b = 11U;
} // namespace sys::abi::v1
