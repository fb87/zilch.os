#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/thread.hh>
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
    inline constexpr sys::word_t failure_badge = 1U << 31U;

    [[noreturn]] void fail(sys::word_t client, sys::word_t stage, sys::word_t error = 0U) noexcept {
        const sys::word_t detail = failure_badge | ((client & 0x7fU) << 24U) |
                                   ((stage & 0xffU) << 16U) | (error & 0xffffU);
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           detail);
        for (;;)
            asm volatile("" ::: "memory");
    }

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
        fail(client, 1U);
    const sys::word_t first_handle = client * handles_per_client;

    {
        constexpr sys::vaddr_t grant_address = 0x2000f000U;
        constexpr sys::word_t read_write = 3U;
        const auto grant = sys::ipc_call(
            endpoint, static_cast<sys::word_t>(sys::abi::v1::memory_server_operation::grant_frame),
            first_handle, received_frame_base);
        if (grant.status != static_cast<sys::word_t>(sys::error_t::success) ||
            grant.message0 != 0U || grant.message1 != sys::abi::v1::maximum_ipc_ool_bytes ||
            grant.message2 != received_frame_base)
            fail(client, 8U);
        const sys::word_t mapped =
            sys::control(sys::abi::v1::control_operation::map_frame, 3U, received_frame_base,
                         grant_address, read_write, 0x100U);
        if (mapped != static_cast<sys::word_t>(sys::error_t::success))
            fail(client, 9U, mapped);
        auto* payload = reinterpret_cast<volatile sys::word_t*>(grant_address);
        const sys::word_t pattern = 0x4f4f4c0000000000ULL | client;
        *payload = pattern;
        if (*payload != pattern)
            fail(client, 10U);
        if (sys::control(sys::abi::v1::control_operation::unmap_frame, 3U, received_frame_base,
                         grant_address) != static_cast<sys::word_t>(sys::error_t::success) ||
            sys::control(sys::abi::v1::control_operation::capability_delete, 0U,
                         received_frame_base) != static_cast<sys::word_t>(sys::error_t::success))
            fail(client, 11U);
        sys::word_t released = 0U;
        if (!request(sys::abi::v1::memory_server_operation::release_frame, first_handle, released))
            fail(client, 12U);
    }

    /* Receiver-selected occupied slots must fail without leaking a frame. */
    {
        const auto rejected = sys::ipc_call(
            endpoint,
            static_cast<sys::word_t>(sys::abi::v1::memory_server_operation::allocate_frame),
            first_handle, endpoint);
        if (rejected.status != static_cast<sys::word_t>(sys::error_t::success) ||
            rejected.message0 != static_cast<sys::word_t>(sys::error_t::busy))
            fail(client, 2U,
                 rejected.status != static_cast<sys::word_t>(sys::error_t::success)
                     ? rejected.status
                     : rejected.message0);
    }

    for (sys::word_t cycle = 0U; cycle < cycles; ++cycle) {
        for (sys::word_t offset = 0U; offset < handles_per_client; ++offset) {
            sys::word_t value = 0U;
            const auto destination =
                static_cast<sys::capability_id_t>(received_frame_base + offset);
            if (!request(sys::abi::v1::memory_server_operation::allocate_frame,
                         first_handle + offset, value, destination) ||
                value != first_handle + offset)
                fail(client, 3U);
        }
        sys::word_t used = 0U;
        if (!request(sys::abi::v1::memory_server_operation::query, 0U, used) || used == 0U ||
            used > sys::abi::v1::memory_server_max_handles)
            fail(client, 4U);
        for (sys::word_t offset = 0U; offset < handles_per_client; ++offset) {
            const auto destination =
                static_cast<sys::capability_id_t>(received_frame_base + offset);
            if (sys::control(sys::abi::v1::control_operation::capability_delete, 0U, destination) !=
                static_cast<sys::word_t>(sys::error_t::success))
                fail(client, 5U);
            sys::word_t value = 0U;
            if (!request(sys::abi::v1::memory_server_operation::release_frame,
                         first_handle + offset, value))
                fail(client, 6U);
        }
    }

    sys::word_t ignored = 0U;
    if (!request(sys::abi::v1::memory_server_operation::shutdown, 0U, ignored))
        fail(client, 7U);
    const sys::word_t badge = 1U << (client + 2U);
    sys::thread_exit(0U, notification, badge);
}
