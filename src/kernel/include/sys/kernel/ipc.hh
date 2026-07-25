#pragma once

#include <sys/kernel/object.hh>

namespace sys::kernel::ipc
{
    inline constexpr usize_t maximum_message_words = 16U;
    inline constexpr usize_t maximum_transferred_capabilities = 4U;

    struct message_tag_t {
        word_t raw;

        [[nodiscard]]
        constexpr usize_t word_count() const noexcept {
            return static_cast<usize_t>(raw & 0x3fU);
        }
    };

    struct message_t {
        message_tag_t tag;
        word_t words[maximum_message_words];
    };

    struct endpoint_t {
        object::header_t header;
        thread_id_t sender;
        thread_id_t receiver;
    };

    [[nodiscard]]
    inline error_t validate(const message_t& message) noexcept {
        return message.tag.word_count() <= maximum_message_words ? error_t::success
                                                                 : error_t::invalid_argument;
    }
} // namespace sys::kernel::ipc
