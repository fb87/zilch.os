#pragma once

#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/serial.hh>

/*
 * The native personality: the programming model a zilch userspace process is
 * born into. Everything here describes what a process already holds when its
 * first instruction runs, and the idioms every server had been restating
 * locally.
 *
 * This exists because six servers had independently written
 * `service_endpoint = 11U`, five had written `root_notification = 14U`, and
 * four had written `failure_badge = 1U << 15U`. Those are not per-server
 * choices -- most are a kernel contract -- so having a copy per server means
 * a change to the contract has six places to miss.
 */
namespace sys::native
{
    /*
     * Installed by the kernel into every child's own cspace, in
     * create_user_bundle() (kernel/thread/scheduler.hh). A process holds
     * these before it runs; nothing in userspace chooses them.
     */
    inline constexpr capability_id_t own_task = 1U;
    inline constexpr capability_id_t own_thread = 2U;
    inline constexpr capability_id_t own_space = 3U;
    inline constexpr capability_id_t own_scheduling_context = 4U;
    /*
     * Minted from root's own slot 10 and badged per-child, so a process
     * sending here reaches root's fault handling identified as itself.
     * Read rights are added only for roles that act as pagers.
     */
    inline constexpr capability_id_t fault_endpoint = 10U;
    /* Write-only capability to root's readiness notification. */
    inline constexpr capability_id_t root_notification = 14U;
    /* Memory resource delegated with the task's page quota. */
    inline constexpr capability_id_t memory_resource = 15U;

    /*
     * NOT a kernel contract: root mints a process's own service endpoint
     * here (root_graph.hh's mint after each process_create). It lives beside
     * the kernel slots because every server needs it, but it is a userspace
     * convention and can be changed by changing root_graph.
     */
    inline constexpr capability_id_t service_endpoint = 11U;

    /*
     * Signalled to root_notification when bring-up fails. Distinct from the
     * per-service ready badges in abi::v1 (bits 0..7), which root sums into
     * its expected mask.
     */
    inline constexpr word_t failure_badge = 1U << 15U;

    [[nodiscard]] inline constexpr bool ok(word_t raw) noexcept {
        return raw == static_cast<word_t>(error_t::success);
    }

    [[nodiscard]] inline constexpr error_t status(word_t raw) noexcept {
        return static_cast<error_t>(static_cast<s64>(raw));
    }

    /*
     * Bounded retry for a capability root mints only after process_create
     * returns: a child can genuinely start and reach its setup code before
     * root's follow-up mint executes, so a one-shot attempt races. Every
     * driver had its own copy of this loop with its own attempt constant.
     */
    inline constexpr word_t default_attempts = 100000U;

    template <typename Attempt>
    [[nodiscard]] inline bool retry(Attempt attempt, word_t attempts = default_attempts) noexcept {
        for (word_t index = 0U; index < attempts; ++index) {
            if (attempt())
                return true;
        }
        return false;
    }

    inline void signal_ready(word_t badge) noexcept {
        (void)control(abi::v1::control_operation::notification_signal, root_notification, badge);
    }

    inline void signal_failure() noexcept {
        (void)control(abi::v1::control_operation::notification_signal, root_notification,
                      failure_badge);
    }

    /*
     * Text output through a capability to serial-driver's service endpoint.
     * Bring-up diagnostics are otherwise invisible: a userspace process has
     * no printk, and the console path is exactly this IPC call. The virtio
     * driver grew a private copy of all three of these; this is that copy,
     * shared.
     */
    namespace text
    {
        inline void byte(capability_id_t serial, char value) noexcept {
            (void)ipc_call(serial,
                           static_cast<word_t>(abi::v1::serial_operation::write_byte),
                           static_cast<word_t>(static_cast<u8>(value)));
        }

        inline void write(capability_id_t serial, const char* value) noexcept {
            for (const char* cursor = value; *cursor != '\0'; ++cursor)
                byte(serial, *cursor);
        }

        inline void hex(capability_id_t serial, u64 value) noexcept {
            byte(serial, '0');
            byte(serial, 'x');
            bool leading = true;
            for (int shift = 60; shift >= 0; shift -= 4) {
                const u64 digit = (value >> static_cast<u64>(shift)) & 0xfULL;
                if (digit == 0U && leading && shift != 0)
                    continue;
                leading = false;
                byte(serial, static_cast<char>(digit < 10U ? '0' + digit : 'a' + (digit - 10U)));
            }
        }

        inline void line(capability_id_t serial, const char* value) noexcept {
            write(serial, value);
            write(serial, "\r\n");
        }
    } // namespace text
} // namespace sys::native
