#pragma once

#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control_plane.hh>

namespace sys::console
{
    /*
     * Sends a short NUL-terminated string (up to
     * abi::v1::console_write_max_bytes - 1 characters -- longer text is
     * truncated, not chunked; multi-call streaming is a later extension,
     * not needed to prove the mechanism) to a console-server endpoint,
     * packed 8 bytes per word into message1..message3. Meaningful only
     * against an endpoint bound to control_plane_role::console.
     */
    [[nodiscard]] inline word_t write(capability_id_t endpoint, const char* text) noexcept {
        word_t words[3]{};
        usize_t index = 0U;
        for (; index < abi::v1::console_write_max_bytes - 1U && text[index] != '\0'; ++index) {
            const usize_t word_index = index / 8U;
            const usize_t byte_index = index % 8U;
            words[word_index] |= static_cast<word_t>(static_cast<u8>(text[index]))
                                  << (byte_index * 8U);
        }
        const auto reply = ipc_call(
            endpoint, static_cast<word_t>(abi::v1::control_plane_operation::write), words[0],
            words[1], words[2]);
        return reply.status;
    }

    /*
     * Single-byte variants for guest vPL011 forwarding (as opposed to
     * write()'s host-diagnostic NUL-terminated string).
     */
    [[nodiscard]] inline word_t write_byte(capability_id_t endpoint, u8 value) noexcept {
        const auto reply = ipc_call(
            endpoint, static_cast<word_t>(abi::v1::control_plane_operation::write_byte),
            static_cast<word_t>(value), 0U, 0U);
        return reply.status;
    }

    struct read_byte_result final {
        bool available{};
        u8 value{};
    };

    [[nodiscard]] inline read_byte_result read_byte(capability_id_t endpoint) noexcept {
        const auto reply = ipc_call(
            endpoint, static_cast<word_t>(abi::v1::control_plane_operation::read_byte), 0U, 0U,
            0U);
        if (reply.status != static_cast<word_t>(error_t::success) || reply.message0 == 0U)
            return {};
        return {true, static_cast<u8>(reply.message1)};
    }
} // namespace sys::console
