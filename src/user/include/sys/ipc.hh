#pragma once

#include <sys/syscall.hh>

#include <abi/sys/v1/ipc.hh>

namespace sys
{
    [[nodiscard]] inline abi::v1::ipc_result ipc_call(capability_id_t endpoint, word_t message0,
                                                      word_t message1, word_t message2 = 0U,
                                                      word_t message3 = 0U,
                                                      abi::v1::ipc_transfer transfer = {},
                                                      abi::v1::ipc_timeout timeout = {}) noexcept {
        return sys_ipc_exchange_raw(endpoint, static_cast<word_t>(abi::v1::ipc_operation::call),
                                    message0, message1, message2, message3, transfer.encode(),
                                    timeout.encode());
    }

    [[nodiscard]] inline abi::v1::ipc_result ipc_receive(capability_id_t endpoint,
                                                         word_t timeout = 0U) noexcept {
        return sys_ipc_exchange_raw(endpoint, static_cast<word_t>(abi::v1::ipc_operation::receive),
                                    0U, 0U, 0U, 0U, 0U, timeout);
    }

    [[nodiscard]] inline word_t ipc_reply(word_t message0, word_t message1, word_t message2,
                                          word_t message3,
                                          abi::v1::ipc_transfer transfer = {}) noexcept {
        return sys_ipc_invoke_raw(0U, static_cast<word_t>(abi::v1::ipc_operation::reply), message0,
                                  message1, message2, message3, transfer.encode(), 0U);
    }

    [[nodiscard]] inline abi::v1::ipc_result
    ipc_reply_receive(capability_id_t endpoint, word_t message0, word_t message1, word_t message2,
                      word_t message3, word_t timeout = 0U,
                      abi::v1::ipc_transfer transfer = {}) noexcept {
        return sys_ipc_exchange_raw(
            endpoint, static_cast<word_t>(abi::v1::ipc_operation::reply_receive), message0,
            message1, message2, message3, transfer.encode(), timeout);
    }
} // namespace sys
