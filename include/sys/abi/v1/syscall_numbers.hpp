#pragma once

#include <sys/types.hpp>

namespace sys::abi::v1
{
    enum class Syscall : u32 {
        ipc_call = 0U,
        ipc_send = 1U,
        ipc_receive = 2U,
        thread_control = 3U,
        space_control = 4U,
        interrupt_control = 5U,
        memory_control = 6U,
        hypervisor_control = 7U
    };
} // namespace sys::abi::v1
