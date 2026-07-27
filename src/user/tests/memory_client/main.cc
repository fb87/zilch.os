#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::word_t endpoint = 10U;
    inline constexpr sys::word_t notification = 14U;
    inline constexpr sys::word_t role_base = 0x103U;
    inline constexpr sys::word_t cycles = 64U;
    inline constexpr sys::word_t handles_per_client = 4U;
    inline constexpr sys::capability_id_t received_frame_base = 20U;

    [[nodiscard]] bool request(sys::abi::v1::memory_server_operation operation, sys::word_t handle,
                               sys::word_t& value, sys::capability_id_t destination = 0U) noexcept {
        const auto result =
            sys::ipc_call(endpoint, static_cast<sys::word_t>(operation), handle, destination);
        value = result.message1;
        return result.status == static_cast<sys::word_t>(sys::error_t::success) &&
               result.message0 == static_cast<sys::word_t>(sys::error_t::success);
    }
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const sys::word_t client = role - role_base;
    if (client >= 3U)
        return 1;
    const sys::word_t first_handle = client * handles_per_client;

    /* Receiver-selected occupied slots must fail without leaking a frame. */
    {
        const auto rejected = sys::ipc_call(
            endpoint,
            static_cast<sys::word_t>(sys::abi::v1::memory_server_operation::allocate_frame),
            first_handle, endpoint);
        if (rejected.status != static_cast<sys::word_t>(sys::error_t::success) ||
            rejected.message0 != static_cast<sys::word_t>(sys::error_t::busy))
            return 2;
    }

    for (sys::word_t cycle = 0U; cycle < cycles; ++cycle) {
        for (sys::word_t offset = 0U; offset < handles_per_client; ++offset) {
            sys::word_t value = 0U;
            const auto destination =
                static_cast<sys::capability_id_t>(received_frame_base + offset);
            if (!request(sys::abi::v1::memory_server_operation::allocate_frame,
                         first_handle + offset, value, destination) ||
                value != first_handle + offset)
                return 3;
        }
        sys::word_t used = 0U;
        if (!request(sys::abi::v1::memory_server_operation::query, 0U, used) || used == 0U ||
            used > sys::abi::v1::memory_server_max_handles)
            return 4;
        for (sys::word_t offset = 0U; offset < handles_per_client; ++offset) {
            const auto destination =
                static_cast<sys::capability_id_t>(received_frame_base + offset);
            if (sys::control(sys::abi::v1::control_operation::capability_delete, 0U, destination) !=
                static_cast<sys::word_t>(sys::error_t::success))
                return 5;
            sys::word_t value = 0U;
            if (!request(sys::abi::v1::memory_server_operation::release_frame,
                         first_handle + offset, value))
                return 6;
        }
    }

    sys::word_t ignored = 0U;
    if (!request(sys::abi::v1::memory_server_operation::shutdown, 0U, ignored))
        return 7;
    const sys::word_t badge = 1U << (client + 2U);
    return sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                        badge) == static_cast<sys::word_t>(sys::error_t::success)
               ? 0
               : 8;
}
