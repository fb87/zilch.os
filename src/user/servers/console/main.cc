#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/control_plane.hh>
#include <abi/sys/v1/serial.hh>

namespace
{
    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    // Dedicated to read_byte, served by a second thread independent from
    // the write/health/describe/stop loop below -- see stdin_main().
    inline constexpr sys::capability_id_t stdin_endpoint = 12U;
    inline constexpr sys::word_t failure_badge = 1U << 15U;

    /*
     * Must match root_graph.hh's console_serial_endpoint_selector /
     * console_stdin_role / console_stdin_thread_selector /
     * console_stdin_space_selector exactly -- root mints/binds these for
     * this process at those numbers before it ever runs, the same
     * convention every other cross-service capability in this codebase
     * already follows (e.g. domain-manager's console_endpoint_selector
     * mirroring root_graph.hh's domain_console_endpoint_selector).
     */
    inline constexpr sys::capability_id_t serial_endpoint = 20U;
    inline constexpr sys::word_t stdin_role = 0x108U;
    inline constexpr sys::capability_id_t stdin_thread_selector = 30U;
    inline constexpr sys::capability_id_t stdin_space_selector = 31U;

    // No hardware access at all anymore -- everything below forwards to
    // serial-driver (src/user/drivers/serial) over IPC. write_byte/
    // read_byte mirror serial_operation's wire shape 1:1; write's raw
    // packed message words are forwarded unchanged (same packing serial-
    // driver expects), so no re-encoding is needed for it.
    namespace serial
    {
        [[nodiscard]] inline sys::word_t write_byte(sys::u8 value) noexcept {
            const auto reply = sys::ipc_call(
                serial_endpoint, static_cast<sys::word_t>(sys::abi::v1::serial_operation::write_byte),
                static_cast<sys::word_t>(value), 0U, 0U);
            return reply.status;
        }

        struct read_byte_result final {
            bool available{};
            sys::u8 value{};
        };

        [[nodiscard]] inline read_byte_result read_byte() noexcept {
            const auto reply = sys::ipc_call(
                serial_endpoint, static_cast<sys::word_t>(sys::abi::v1::serial_operation::read_byte),
                0U, 0U, 0U);
            if (reply.status != static_cast<sys::word_t>(sys::error_t::success) ||
                reply.message0 == 0U)
                return {};
            return {true, static_cast<sys::u8>(reply.message1)};
        }
    } // namespace serial

    // The second thread (thread_create'd by main() below), independent
    // from the write-serving loop: only ever forwards read_byte to
    // serial-driver's own RX ring buffer and replies. No state shared with
    // the write thread -- each only forwards its own op type and replies,
    // so no coordination beyond the capabilities root already minted for
    // each endpoint is needed.
    [[noreturn]] inline void stdin_main() noexcept {
        for (;;) {
            const auto request = sys::ipc_receive(stdin_endpoint);
            if (request.status != static_cast<sys::word_t>(sys::error_t::success))
                continue;
            const auto operation =
                static_cast<sys::abi::v1::control_plane_operation>(request.message0);
            sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
            sys::word_t result1 = 0U;
            if (operation == sys::abi::v1::control_plane_operation::read_byte) {
                const auto polled = serial::read_byte();
                result0 = polled.available ? 1U : 0U;
                result1 = polled.available ? polled.value : 0U;
            }
            (void)sys::ipc_reply(result0, result1, 0U, 0U);
        }
    }
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    if (role == stdin_role)
        stdin_main(); // noreturn

    const auto policy = sys::control_plane::policy_for(role);
    const sys::word_t ready = sys::abi::v1::control_plane_ready_badge(role);
    if (!sys::control_plane::valid(policy) || ready == 0U) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, root_notification,
                           failure_badge);
        return 1;
    }

    /*
     * Root mints stdin_endpoint into this cspace only AFTER process_create
     * returns it a task capability, so this process genuinely can (and
     * does) start running first -- the same ordering window
     * root_graph.hh's launch() loop already documents for its own mints.
     * The stdin thread must not be spawned before that mint lands: its
     * receive loop would fail capability resolution immediately rather
     * than blocking, and spin hot on the failure, starving whatever else
     * shares its CPU. That was observed hanging boot outright (root never
     * got far enough to launch the remaining roles). Waiting here for the
     * capability to resolve keeps the spawn correct by construction;
     * timed_out means the endpoint exists and is simply idle, which is the
     * success signal.
     */
    constexpr sys::word_t stdin_endpoint_attempts = 100000U;
    bool stdin_ready = false;
    for (sys::word_t attempt = 0U; !stdin_ready && attempt < stdin_endpoint_attempts; ++attempt) {
        const auto probe = sys::ipc_receive(stdin_endpoint, sys::abi::v1::encode_timeout(1U));
        stdin_ready = probe.status != static_cast<sys::word_t>(sys::error_t::not_found) &&
                      probe.status != static_cast<sys::word_t>(sys::error_t::denied);
    }

    // thread_create is not root-gated (only role_image_bind, done for us
    // once by root_graph.hh's bind_role_images(), is) -- this process
    // self-services spawning its own second thread, same shape as
    // root_graph.hh's spawn_supervision_thread() but invoked by the role
    // itself rather than by root. CPU 1 deliberately: not root's CPU 0,
    // and not this server's own (console is index 2, so `index % 4` pins
    // its main thread to CPU 2).
    if (!stdin_ready ||
        sys::control(sys::abi::v1::control_operation::thread_create, 1U, stdin_role,
                     stdin_thread_selector, stdin_space_selector) !=
            static_cast<sys::word_t>(sys::error_t::success)) {
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
            // Only exits this (write) thread -- thread_exit is a per-thread
            // terminal transition. The stdin thread is left running; stop
            // is unused by the production restart path today (restart_
            // role() destroys the whole task/bundle directly), so this
            // asymmetry has no live caller to matter to yet.
            sys::thread_exit(0U, root_notification, sys::abi::v1::control_plane_exit_badge(role));
        } else if (operation == sys::abi::v1::control_plane_operation::write) {
            const auto reply = sys::ipc_call(
                serial_endpoint, static_cast<sys::word_t>(sys::abi::v1::serial_operation::write),
                request.message1, request.message2, request.message3);
            result0 = reply.status;
        } else if (operation == sys::abi::v1::control_plane_operation::write_byte) {
            result0 = serial::write_byte(static_cast<sys::u8>(request.message1 & 0xffU));
        }
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
