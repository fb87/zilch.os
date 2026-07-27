#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::word_t endpoint = 10U;
    inline constexpr sys::word_t notification = 14U;
    inline constexpr sys::word_t working_frame = 12U;
    inline constexpr sys::word_t client_count = 2U;
    inline constexpr sys::word_t completion_magic = 0x50414745U;
    inline constexpr sys::word_t failure_badge_base = 1U << 16U;

    [[noreturn]] void fail(sys::word_t code) noexcept {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                           failure_badge_base | code);
        for (;;)
            asm volatile("" ::: "memory");
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    for (sys::word_t index = 0U; index < client_count; ++index) {
        const sys::word_t created = sys::control(
            sys::abi::v1::control_operation::resource_frame_create, 15U, working_frame);
        if (created != static_cast<sys::word_t>(sys::error_t::success))
            fail(1U + index * 8U);

        const auto fault = sys::ipc_receive(endpoint);
        if (fault.status != static_cast<sys::word_t>(sys::error_t::success))
            fail(2U + index * 8U);

        constexpr auto read_write =
            sys::abi::v1::memory_permission::read | sys::abi::v1::memory_permission::write;
        const sys::word_t resolved =
            sys::control(sys::abi::v1::control_operation::fault_reply_sender, working_frame,
                         fault.message2, sys::abi::v1::encode(read_write));
        if (resolved != static_cast<sys::word_t>(sys::error_t::success))
            fail(3U + index * 8U);

        const auto completion = sys::ipc_receive(endpoint);
        if (completion.status != static_cast<sys::word_t>(sys::error_t::success) ||
            completion.message0 != completion_magic)
            fail(4U + index * 8U);

        const sys::word_t reclaimed =
            sys::control(sys::abi::v1::control_operation::pager_reclaim_sender, working_frame,
                         completion.message1);
        if (reclaimed != static_cast<sys::word_t>(sys::error_t::success))
            fail(5U + index * 8U);

        /*
         * Complete the client's synchronous call before publishing its badge.
         * This prevents root from destroying/reusing the client thread while
         * the server still holds a reply capability to the old generation.
         */
        const sys::word_t replied = sys::ipc_reply(0U, 0U, 0U, 0U);
        if (replied != static_cast<sys::word_t>(sys::error_t::success))
            fail(6U + index * 8U);

        const sys::word_t signalled =
            sys::control(sys::abi::v1::control_operation::notification_signal, notification,
                         completion.message2);
        if (signalled != static_cast<sys::word_t>(sys::error_t::success))
            fail(7U + index * 8U);
    }
    return 0;
}
