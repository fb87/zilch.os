#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

namespace
{
    // Single source of truth in the native personality; see
    // src/user/personalities/native/README.md.
    inline constexpr sys::capability_id_t root_notification = sys::native::root_notification;
    inline constexpr sys::capability_id_t service_endpoint = sys::native::service_endpoint;
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const auto policy = sys::control_plane::policy_for(role);
    const sys::word_t ready = sys::abi::v1::control_plane_ready_badge(role);
    if (!sys::control_plane::valid(policy) || ready == 0U) {
        sys::native::signal_failure();
        return 1;
    }

    sys::native::signal_ready(ready);

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
#if CONFIG_FAULT_INJECTION
        } else if (operation == sys::abi::v1::control_plane_operation::inject_fault) {
            /*
             * Deliberate null dereference: a data abort the kernel reports
             * to root's fault endpoint exactly like any genuine crash, so
             * the restart path under test is the real one rather than a
             * cooperative shutdown. No reply is sent -- the caller times out.
             */
            *reinterpret_cast<volatile sys::u64*>(0) = 0U;
            return 4;
#endif
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
