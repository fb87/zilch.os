#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

namespace
{
    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    inline constexpr sys::word_t failure_badge = 1U << 15U;
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const auto policy = sys::control_plane::policy_for(role);
    const sys::word_t ready = sys::abi::v1::control_plane_ready_badge(role);
    if (!sys::control_plane::valid(policy) || ready == 0U) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, root_notification,
                           failure_badge);
        return 1;
    }

    const sys::word_t status = sys::control(sys::abi::v1::control_operation::notification_signal,
                                            root_notification, ready);
    if (status != static_cast<sys::word_t>(sys::error_t::success))
        return 2;

    for (;;) {
        const auto request = sys::ipc_receive(service_endpoint);
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<sys::abi::v1::control_plane_operation>(request.message0);
        sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        sys::word_t result1 = 0U;
        if (operation == sys::abi::v1::control_plane_operation::health) {
            result0 = sys::abi::v1::control_plane_health_magic;
            result1 = role;
        } else if (operation == sys::abi::v1::control_plane_operation::describe) {
            result0 = policy.dependency_mask;
            result1 = (policy.quota_pages << 16U) | policy.restart_limit;
        } else if (operation == sys::abi::v1::control_plane_operation::stop) {
            const sys::word_t replied = sys::ipc_reply(0U, role, 0U, 0U);
            if (replied != static_cast<sys::word_t>(sys::error_t::success))
                return 3;
            sys::thread_exit(0U, root_notification, sys::abi::v1::control_plane_exit_badge(role));
        }
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
