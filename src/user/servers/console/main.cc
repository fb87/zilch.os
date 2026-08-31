#include <sys/control.hh>
#include <sys/control_plane.hh>
#include <sys/ipc.hh>
#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

namespace
{
    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    inline constexpr sys::word_t failure_badge = 1U << 15U;

    /*
     * device_frame_create() is root-gated; this server cannot call it
     * itself. root_graph.hh creates the UART device frame and mints it
     * into this slot before this process ever runs (mirroring how
     * start_embedded_guest() mints device frames into the domain-manager)
     * -- this server only ever maps a capability that is already there.
     */
    inline constexpr sys::capability_id_t uart_frame_selector = 20U;
    inline constexpr sys::capability_id_t self_space_selector = 3U;
    inline constexpr sys::word_t uart_scratch_address = 0x2003f000U;

    // Same PL011 register layout sys::platform::console::putc() uses
    // (src/platform/qemu_arm64_virt/include/sys/platform/console.hh) --
    // duplicated here because userspace has no access to platform:: headers.
    inline constexpr sys::uintptr_t data_offset = 0x00U;
    inline constexpr sys::uintptr_t control_offset = 0x30U;
    inline constexpr sys::uintptr_t flag_offset = 0x18U;
    inline constexpr sys::u32 transmit_fifo_full = 1U << 5U;
    inline constexpr sys::u32 receive_fifo_empty = 1U << 4U;
    inline constexpr sys::u32 cr_uarten = 1U << 0U;
    inline constexpr sys::u32 cr_txe = 1U << 8U;
    inline constexpr sys::u32 cr_rxe = 1U << 9U;

    /*
     * Previously the guest's own driver wrote this directly to real
     * hardware (direct passthrough). Now this server is the exclusive
     * owner and nothing else ever touches real hardware, so it must do
     * this itself -- QEMU's PL011 model happens to accept TX writes
     * regardless, which is why TX appeared to work even before this was
     * added, but RX is gated behind CR.RXE and silently never captured
     * anything without it.
     */
    inline void configure_uart() noexcept {
        auto* control = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + control_offset);
        *control = cr_uarten | cr_txe | cr_rxe;
    }

    inline void putc(char value) noexcept {
        auto* data = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + data_offset);
        auto* flags = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + flag_offset);
        while ((*flags & transmit_fifo_full) != 0U) {
        }
        *data = static_cast<sys::u32>(static_cast<sys::u8>(value));
    }

    // Non-blocking: returns false immediately if the real hardware RX FIFO
    // is empty. Used to opportunistically fill the one-byte pending queue
    // below, never to wait for input.
    [[nodiscard]] inline bool try_getc(sys::u8& value) noexcept {
        auto* data = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + data_offset);
        auto* flags = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + flag_offset);
        if ((*flags & receive_fifo_empty) != 0U)
            return false;
        value = static_cast<sys::u8>(*data);
        return true;
    }

    // One-byte pending RX queue, polled from real hardware once per main
    // loop iteration (see poll_input() below) and drained by read_byte.
    // A single byte is sufficient: the domain-manager's own vPL011
    // emulation polls read_byte once per serve() iteration too, so bytes
    // are consumed roughly as fast as they arrive; deeper buffering is a
    // throughput concern, not a correctness one, for an interactive shell.
    bool rx_pending = false;
    sys::u8 rx_byte = 0U;

    inline void poll_input() noexcept {
        if (rx_pending)
            return;
        sys::u8 value = 0U;
        if (try_getc(value)) {
            rx_pending = true;
            rx_byte = value;
        }
    }

    // root creates and mints the UART device frame into uart_frame_selector
    // only after this process already exists (capability_mint's
    // destination-task capability can't be minted into before
    // process_create returns it) -- so this process can genuinely start
    // running and reach here before root's mint call lands. Bounded retry,
    // not a one-shot attempt, closes that ordering window instead of
    // relying on root's mint happening to win the race in practice.
    inline constexpr sys::word_t map_uart_attempts = 100000U;

    [[nodiscard]] inline bool map_uart() noexcept {
        const sys::word_t read_write = static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
                                       static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::device, sys::abi::v1::memory_shareability::non_shareable);
        for (sys::word_t attempt = 0U; attempt < map_uart_attempts; ++attempt) {
            if (sys::control(sys::abi::v1::control_operation::map_frame, self_space_selector,
                             uart_frame_selector, uart_scratch_address, read_write, attrs) ==
                static_cast<sys::word_t>(sys::error_t::success))
                return true;
        }
        return false;
    }
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const auto policy = sys::control_plane::policy_for(role);
    const sys::word_t ready = sys::abi::v1::control_plane_ready_badge(role);
    if (!sys::control_plane::valid(policy) || ready == 0U || !map_uart()) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, root_notification,
                           failure_badge);
        return 1;
    }
    configure_uart();

    const sys::word_t status = sys::control(sys::abi::v1::control_operation::notification_signal,
                                            root_notification, ready);
    if (status != static_cast<sys::word_t>(sys::error_t::success))
        return 2;

    for (;;) {
        poll_input();
        /*
         * A bounded timeout (not an indefinite blocking receive) so this
         * loop keeps polling real hardware RX between requests -- matching
         * the pattern already used for root_graph.hh's
         * drain_fault_reports(). A short wait is enough to keep input
         * latency low without busy-spinning.
         */
        const auto request =
            sys::ipc_receive(service_endpoint, sys::abi::v1::encode_timeout(1U));
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
        } else if (operation == sys::abi::v1::control_plane_operation::write) {
            const sys::word_t words[3] = {request.message1, request.message2, request.message3};
            for (sys::usize_t index = 0U; index < sys::abi::v1::console_write_max_bytes - 1U;
                ++index) {
                const sys::usize_t word_index = index / 8U;
                const sys::usize_t byte_index = index % 8U;
                const char value =
                    static_cast<char>((words[word_index] >> (byte_index * 8U)) & 0xffU);
                if (value == '\0')
                    break;
                putc(value);
            }
            result0 = static_cast<sys::word_t>(sys::error_t::success);
        } else if (operation == sys::abi::v1::control_plane_operation::write_byte) {
            putc(static_cast<char>(request.message1 & 0xffU));
            result0 = static_cast<sys::word_t>(sys::error_t::success);
        } else if (operation == sys::abi::v1::control_plane_operation::read_byte) {
            poll_input();
            result0 = rx_pending ? 1U : 0U;
            result1 = rx_pending ? rx_byte : 0U;
            rx_pending = false;
        }
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
