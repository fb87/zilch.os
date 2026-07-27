#pragma once

#include <sys/types.hh>

#include <abi/sys/v1/message.hh>
#include <abi/sys/v1/syscall_numbers.hh>

namespace sys::abi::v1
{
    struct ipc_transfer final {
        capability_id_t source{static_cast<capability_id_t>(-1)};
        capability_id_t destination{static_cast<capability_id_t>(-1)};
        u32 rights{};
        u32 badge{};

        [[nodiscard]] constexpr word_t encode() const noexcept {
            return source == static_cast<capability_id_t>(-1)
                       ? 0U
                       : encode_capability_transfer(source, destination, rights, badge);
        }
    };

    struct ipc_timeout final {
        u64 ticks{};
        bool enabled{};

        [[nodiscard]] constexpr word_t encode() const noexcept {
            return enabled ? encode_timeout(ticks) : no_timeout;
        }
    };
} // namespace sys::abi::v1

extern "C" sys::word_t sys_ipc_invoke_raw(sys::word_t endpoint, sys::word_t operation,
                                          sys::word_t message0, sys::word_t message1,
                                          sys::word_t message2, sys::word_t message3,
                                          sys::word_t transfer_descriptor,
                                          sys::word_t timeout_descriptor) noexcept;

namespace sys::abi::v1
{
    struct ipc_result final {
        word_t status{};
        word_t sender{};
        word_t message0{};
        word_t message1{};
        word_t message2{};
        word_t message3{};
    };
} // namespace sys::abi::v1

sys::abi::v1::ipc_result sys_ipc_exchange_raw(sys::word_t endpoint, sys::word_t operation,
                                              sys::word_t message0, sys::word_t message1,
                                              sys::word_t message2, sys::word_t message3,
                                              sys::word_t transfer_descriptor,
                                              sys::word_t timeout_descriptor) noexcept;
