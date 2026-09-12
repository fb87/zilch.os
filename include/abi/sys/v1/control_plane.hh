#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    enum class control_plane_role : word_t {
        process = 0x200U,
        device = 0x201U,
        console = 0x202U,
        domain = 0x203U,
        supervisor = 0x204U,
    };

    enum class control_plane_operation : word_t {
        health = 0U,
        describe = 1U,
        stop = 2U,
        launch = 3U,
        destroy = 4U,
        load = 5U,
        run = 6U,
        serve = 7U,
        // Console-role-specific: writes up to console_write_max_bytes packed
        // into message1..message3 (8 bytes per word, little-endian), NUL-
        // padded if shorter, to the console server's owned UART. Meaningful
        // only for control_plane_role::console, same pattern as
        // load/run/serve being meaningful only for control_plane_role::domain.
        write = 8U,
        // Console-role-specific, single-byte variants for guest vPL011
        // forwarding (as opposed to write's host-diagnostic NUL-terminated
        // string): write_byte's message1 (low byte) is forwarded to the
        // owned UART; read_byte takes no payload and replies message0 = 1
        // if a byte was available (0 otherwise), message1 = the byte.
        write_byte = 9U,
        read_byte = 10U,
        /*
         * Makes the receiving role fault deliberately, so restart-on-fault
         * can be verified. Only honoured when CONFIG_FAULT_INJECTION is set;
         * otherwise it falls through to invalid_argument like any unknown
         * operation. The caller gets no reply -- the role dies before
         * replying -- so it must use a timeout.
         */
        inject_fault = 11U,
        // Same wire shape and reply contract as read_byte, but blocks
        // until a byte is actually available instead of replying
        // immediately with "none available" -- see serial_operation::
        // read_byte_wait, which this forwards to. Used only by the
        // interactive shell's read() via console_client.hh; domain-manager
        // must keep using plain read_byte (its non-blocking contract is
        // load-bearing for VM idle-exit handling).
        read_byte_wait = 12U,
    };

    inline constexpr usize_t console_write_max_bytes = 24U;

    inline constexpr word_t control_plane_role_count = 5U;
    inline constexpr word_t memory_service_ready_badge = 1U << 5U;
    inline constexpr word_t serial_service_ready_badge = 1U << 6U;
    inline constexpr word_t block_service_ready_badge = 1U << 7U;
    inline constexpr word_t vfs_service_ready_badge = 1U << 8U;
    inline constexpr word_t control_plane_health_magic = 0x4845414cU;
    /*
     * Bits 9 upward, and that base is load-bearing rather than arbitrary.
     *
     * These used to start at bit 8, which put the FIRST role's exit badge on
     * the same bit as vfs_service_ready_badge. Root would have read a
     * process-role exit as "VFS is ready", and a VFS readiness signal as
     * "the process role exited" -- in a shared notification word where both
     * are accumulated. It went unnoticed because root observed only the
     * failure badge and never looked at exit badges at all (USR-033), so the
     * collision had nothing to collide with in practice. The static_asserts
     * below keep every class disjoint by construction now that root does
     * read them.
     */
    inline constexpr word_t control_plane_exit_badge_base = 9U;

    inline constexpr word_t control_plane_exit_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (control_plane_exit_badge_base + role - first)
                   : 0U;
    }
    inline constexpr word_t control_plane_ready_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (role - first)
                   : 0U;
    }

    /*
     * Every badge class shares one notification word, so they must be
     * disjoint. Checked here rather than trusted: the exit badges silently
     * overlapped vfs_service_ready_badge until root started reading them.
     *
     * 1U << 15U is native::failure_badge, which this header cannot include
     * (the personality includes this one), so it is written out.
     */
    inline constexpr word_t control_plane_ready_badge_mask =
        ((word_t{1U} << control_plane_role_count) - 1U) | memory_service_ready_badge |
        serial_service_ready_badge | block_service_ready_badge | vfs_service_ready_badge;
    inline constexpr word_t control_plane_exit_badge_mask =
        ((word_t{1U} << control_plane_role_count) - 1U) << control_plane_exit_badge_base;

    static_assert((control_plane_ready_badge_mask & control_plane_exit_badge_mask) == 0U,
                  "exit badges must not overlap readiness badges");
    static_assert((control_plane_ready_badge_mask & (word_t{1U} << 15U)) == 0U,
                  "readiness badges must not overlap the failure badge");
    static_assert((control_plane_exit_badge_mask & (word_t{1U} << 15U)) == 0U,
                  "exit badges must not overlap the failure badge");
    /*
     * The upper 16 bits of this word are reserved: the certification
     * harness treats any badge set there as an unexpected-badge failure
     * (`badges & 0xffff0000`). That was an implicit contract this header did
     * not know about, and placing the exit badges at bit 16 tripped it
     * immediately -- userspace_control_plane_graph failed outright. Named
     * and asserted here so the next badge class cannot repeat it.
     */
    inline constexpr word_t control_plane_badge_reserved_mask = 0xffff0000U;
    static_assert((control_plane_exit_badge_mask & control_plane_badge_reserved_mask) == 0U,
                  "badge classes must stay inside the low 16 bits");
    static_assert((control_plane_ready_badge_mask & control_plane_badge_reserved_mask) == 0U,
                  "badge classes must stay inside the low 16 bits");
} // namespace sys::abi::v1
