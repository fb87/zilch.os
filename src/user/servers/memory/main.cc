#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::word_t endpoint = 10U;
    inline constexpr sys::word_t notification = 14U;
    inline constexpr sys::word_t resource = 15U;
    inline constexpr sys::word_t pager_frame = 12U;
    inline constexpr sys::word_t service_frame_base = 16U;
    inline constexpr sys::word_t pager_client_count = 2U;
    inline constexpr sys::word_t pressure_client_count = 3U;
    inline constexpr sys::word_t completion_magic = 0x50414745U;
    inline constexpr sys::word_t failure_badge_base = 1U << 16U;

    struct handle_state final {
        bool allocated{};
        sys::word_t owner{};
        sys::capability_id_t destination{};
    };

    handle_state handles[sys::abi::v1::memory_server_max_handles]{};

    [[noreturn]] void fail(sys::word_t code) noexcept {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           failure_badge_base | code);
        for (;;)
            asm volatile("" ::: "memory");
    }

    [[nodiscard]] sys::word_t service_request(const sys::abi::v1::ipc_result& request,
                                              sys::word_t& value,
                                              sys::abi::v1::ipc_transfer& transfer) noexcept {
        const auto operation = static_cast<sys::abi::v1::memory_server_operation>(request.message0);
        const sys::word_t handle = request.message1;
        const auto destination = static_cast<sys::capability_id_t>(request.message2);
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        const sys::word_t invalid = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        const sys::word_t denied = static_cast<sys::word_t>(sys::error_t::denied);

        switch (operation) {
            case sys::abi::v1::memory_server_operation::allocate_frame:
                if (handle >= sys::abi::v1::memory_server_max_handles || handles[handle].allocated)
                    return invalid;
                if (sys::control(sys::abi::v1::control_operation::resource_frame_create, resource,
                                 service_frame_base + handle) != success)
                    return static_cast<sys::word_t>(sys::error_t::no_memory);
                handles[handle].allocated = true;
                handles[handle].owner = request.sender;
                handles[handle].destination = destination;
                transfer.source = static_cast<sys::capability_id_t>(service_frame_base + handle);
                transfer.destination = destination;
                transfer.rights = static_cast<sys::u32>(sys::abi::v1::CapabilityRight::read) |
                                  static_cast<sys::u32>(sys::abi::v1::CapabilityRight::write);
                value = handle;
                return success;

            case sys::abi::v1::memory_server_operation::release_frame:
                if (handle >= sys::abi::v1::memory_server_max_handles || !handles[handle].allocated)
                    return invalid;
                if (handles[handle].owner != request.sender)
                    return denied;
                if (sys::control(sys::abi::v1::control_operation::frame_destroy,
                                 service_frame_base + handle) != success)
                    return static_cast<sys::word_t>(sys::error_t::busy);
                handles[handle] = {};
                value = handle;
                return success;

            case sys::abi::v1::memory_server_operation::query:
                return sys::control_result1(
                    value, sys::abi::v1::control_operation::memory_resource_query, resource);

            case sys::abi::v1::memory_server_operation::shutdown:
                for (sys::word_t index = 0U; index < sys::abi::v1::memory_server_max_handles;
                     ++index) {
                    if (handles[index].allocated && handles[index].owner == request.sender)
                        return static_cast<sys::word_t>(sys::error_t::busy);
                }
                return success;
        }
        return invalid;
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    for (sys::word_t index = 0U; index < pager_client_count; ++index) {
        const sys::word_t created = sys::control(
            sys::abi::v1::control_operation::resource_frame_create, resource, pager_frame);
        if (created != static_cast<sys::word_t>(sys::error_t::success))
            fail(1U + index * 8U);

        const auto fault = sys::ipc_receive(endpoint);
        if (fault.status != static_cast<sys::word_t>(sys::error_t::success))
            fail(2U + index * 8U);

        constexpr auto read_write =
            sys::abi::v1::memory_permission::read | sys::abi::v1::memory_permission::write;
        const sys::word_t resolved =
            sys::control(sys::abi::v1::control_operation::fault_reply_sender, pager_frame,
                         fault.message2, sys::abi::v1::encode(read_write));
        if (resolved != static_cast<sys::word_t>(sys::error_t::success))
            fail(3U + index * 8U);

        const auto completion = sys::ipc_receive(endpoint);
        if (completion.status != static_cast<sys::word_t>(sys::error_t::success) ||
            completion.message0 != completion_magic)
            fail(4U + index * 8U);

        const sys::word_t reclaimed =
            sys::control(sys::abi::v1::control_operation::pager_reclaim_sender, pager_frame,
                         completion.message1);
        if (reclaimed != static_cast<sys::word_t>(sys::error_t::success))
            fail(5U + index * 8U);

        const sys::word_t replied = sys::ipc_reply(0U, 0U, 0U, 0U);
        if (replied != static_cast<sys::word_t>(sys::error_t::success))
            fail(6U + index * 8U);

        const sys::word_t signalled =
            sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                         completion.message2);
        if (signalled != static_cast<sys::word_t>(sys::error_t::success))
            fail(7U + index * 8U);
    }

    sys::word_t completed_clients = 0U;
    while (completed_clients < pressure_client_count) {
        const auto request = sys::ipc_receive(endpoint);
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            fail(0x40U);
        sys::word_t value = 0U;
        sys::abi::v1::ipc_transfer transfer{};
        const sys::word_t status = service_request(request, value, transfer);
        sys::word_t reply_status = sys::ipc_reply(status, value, 0U, 0U, transfer);
        if (reply_status != static_cast<sys::word_t>(sys::error_t::success)) {
            /*
             * Capability transfer is atomic with reply delivery.  If the
             * receiver-selected slot is invalid or occupied, reclaim the
             * server-side frame and use the still-live reply authority to
             * report the transfer error.
             */
            if (transfer.source != static_cast<sys::capability_id_t>(-1) &&
                value < sys::abi::v1::memory_server_max_handles && handles[value].allocated) {
                (void)sys::control(sys::abi::v1::control_operation::frame_destroy,
                                   service_frame_base + value);
                handles[value] = {};
            }
            reply_status = sys::ipc_reply(reply_status, 0U, 0U, 0U);
        }
        if (reply_status != static_cast<sys::word_t>(sys::error_t::success))
            fail(0x41U);
        if (static_cast<sys::abi::v1::memory_server_operation>(request.message0) ==
                sys::abi::v1::memory_server_operation::shutdown &&
            status == static_cast<sys::word_t>(sys::error_t::success))
            ++completed_clients;
    }
    return 0;
}
