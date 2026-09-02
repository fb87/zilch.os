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
    };

    inline constexpr usize_t console_write_max_bytes = 24U;

    inline constexpr word_t control_plane_role_count = 5U;
    inline constexpr word_t memory_service_ready_badge = 1U << 5U;
    inline constexpr word_t serial_service_ready_badge = 1U << 6U;
    inline constexpr word_t block_service_ready_badge = 1U << 7U;
    inline constexpr word_t control_plane_health_magic = 0x4845414cU;
    inline constexpr word_t control_plane_exit_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (8U + role - first)
                   : 0U;
    }
    inline constexpr word_t control_plane_ready_badge(word_t role) noexcept {
        const word_t first = static_cast<word_t>(control_plane_role::process);
        return role >= first && role < first + control_plane_role_count
                   ? word_t{1U} << (role - first)
                   : 0U;
    }
} // namespace sys::abi::v1
