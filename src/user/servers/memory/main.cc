#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/memory_service.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::word_t service_endpoint = sys::native::service_endpoint;
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    if (sys::control(sys::abi::v1::control_operation::notification_signal,
                     sys::memory_service::notification, sys::abi::v1::memory_service_ready_badge) !=
        static_cast<sys::word_t>(sys::error_t::success))
        return 1;

    for (;;) {
        const auto request = sys::ipc_receive(service_endpoint);
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        sys::word_t value = 0U;
        sys::abi::v1::ipc_transfer transfer{};
        const sys::word_t status = sys::memory_service::service_request(request, value, transfer);
        const auto operation = static_cast<sys::abi::v1::memory_server_operation>(request.message0);
        sys::word_t reply_status = 0U;
        if (operation == sys::abi::v1::memory_server_operation::grant_frame &&
            status == static_cast<sys::word_t>(sys::error_t::success)) {
            const sys::abi::v1::ipc_ool_message grant{transfer.source,
                                                      transfer.destination,
                                                      transfer.rights,
                                                      transfer.badge,
                                                      0U,
                                                      sys::abi::v1::maximum_ipc_ool_bytes};
            reply_status = sys::ipc_reply_ool(grant);
        } else {
            reply_status = sys::ipc_reply(status, value, 0U, 0U, transfer);
        }
        if (reply_status != static_cast<sys::word_t>(sys::error_t::success) &&
            transfer.source != static_cast<sys::capability_id_t>(-1) &&
            value < sys::abi::v1::memory_server_max_handles &&
            sys::memory_service::handles[value].allocated) {
            (void)sys::control(sys::abi::v1::control_operation::frame_destroy,
                               sys::memory_service::service_frame_base + value);
            sys::memory_service::handles[value] = {};
            (void)sys::ipc_reply(reply_status, 0U, 0U, 0U);
        }
    }
}
