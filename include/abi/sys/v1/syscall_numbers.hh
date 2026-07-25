#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class syscall : word_t
    {
        ipc = 0U,
    };

    enum class ipc_operation : word_t
    {
        call = 0U,
    };

    inline constexpr capability_id_t debug_endpoint = 1U;
} // namespace sys::abi::v1
