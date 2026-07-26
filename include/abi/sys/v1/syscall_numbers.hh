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
    };

    enum class fuzz_case : word_t {
        valid_call = 0U,
        invalid_capability = 1U,
        invalid_operation = 2U,
        wrong_thread_identity = 3U,
        random_payload = 4U,
        boundary_capability = 5U,
        mixed = 6U,
    };

    inline constexpr capability_id_t debug_endpoint = 1U;
    inline constexpr capability_id_t fuzz_endpoint = 2U;
    inline constexpr capability_id_t ipc_server_a = 10U;
    inline constexpr capability_id_t ipc_server_b = 11U;
    inline constexpr word_t fuzz_magic = 0x5a46555a5aULL;
} // namespace sys::abi::v1
