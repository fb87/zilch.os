#pragma once

#include <sys/syscall.hh>

#include <abi/sys/v1/ipc.hh>

namespace sys
{
    [[nodiscard]] inline abi::v1::ipc_result ipc_receive(capability_id_t endpoint,
                                                         word_t timeout = 0U) noexcept {
        return sys_ipc_exchange_raw(endpoint, static_cast<word_t>(abi::v1::ipc_operation::receive),
                                    0U, 0U, 0U, 0U, 0U, timeout);
    }

    [[nodiscard]] inline abi::v1::ipc_result ipc_reply_receive(capability_id_t endpoint,
                                                               word_t message0, word_t message1,
                                                               word_t message2, word_t message3,
                                                               word_t timeout = 0U) noexcept {
        return sys_ipc_exchange_raw(endpoint,
                                    static_cast<word_t>(abi::v1::ipc_operation::reply_receive),
                                    message0, message1, message2, message3, 0U, timeout);
    }
} // namespace sys
