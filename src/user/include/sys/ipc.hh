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

    [[nodiscard]] inline abi::v1::ipc_result
    ipc_call_ool(capability_id_t endpoint, const abi::v1::ipc_ool_message& message,
                 abi::v1::ipc_timeout timeout = {}) noexcept {
        if (!message.valid())
            return {static_cast<word_t>(static_cast<s64>(error_t::invalid_argument))};
        return ipc_call(endpoint, message.offset, message.length, message.destination, 0U,
                        message.transfer(), timeout);
    }

    [[nodiscard]] inline word_t ipc_reply_ool(const abi::v1::ipc_ool_message& message) noexcept {
        if (!message.valid())
            return static_cast<word_t>(static_cast<s64>(error_t::invalid_argument));
        return ipc_reply(message.offset, message.length, message.destination, 0U,
                         message.transfer());
    }
} // namespace sys
