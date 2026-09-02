#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/control_plane.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/serial.hh>

namespace
{
    namespace native = sys::native;

    // Well-known slots and the ready/failure protocol come from the native
    // personality rather than being restated here.
    inline constexpr sys::capability_id_t service_endpoint = native::service_endpoint;

    /*
     * root creates the one live UART device frame and IRQ capability
     * (both root-gated) and mints them into these slots before this
     * process ever runs -- see root_graph.hh's create_serial_uart_frame()/
     * create_serial_irq()/mint_serial_resources(). This process only ever
     * maps/binds capabilities that are already there.
     */
    inline constexpr sys::capability_id_t uart_frame_selector = 20U;
    inline constexpr sys::capability_id_t irq_selector = 21U;
    inline constexpr sys::capability_id_t irq_notification_selector = 22U;
    inline constexpr sys::capability_id_t self_space_selector = native::own_space;
    inline constexpr sys::word_t uart_scratch_address = 0x2003f000U;

    // Same PL011 register layout console-server used to poke directly
    // (src/platform/qemu_arm64_virt/include/sys/platform/console.hh) --
    // duplicated here because userspace has no access to platform:: headers.
    inline constexpr sys::uintptr_t data_offset = 0x00U;
    inline constexpr sys::uintptr_t flag_offset = 0x18U;
    inline constexpr sys::uintptr_t control_offset = 0x30U;
    inline constexpr sys::uintptr_t imsc_offset = 0x38U;
    inline constexpr sys::uintptr_t icr_offset = 0x44U;
    inline constexpr sys::u32 transmit_fifo_full = 1U << 5U;
    inline constexpr sys::u32 receive_fifo_empty = 1U << 4U;
    inline constexpr sys::u32 cr_uarten = 1U << 0U;
    inline constexpr sys::u32 cr_txe = 1U << 8U;
    inline constexpr sys::u32 cr_rxe = 1U << 9U;
    inline constexpr sys::u32 imsc_rxim = 1U << 4U;
    inline constexpr sys::u32 icr_rxic = 1U << 4U;

    /*
     * CR unchanged from what console-server used to write itself (QEMU's
     * PL011 model accepts TX regardless of CR.TXE, but RX is gated behind
     * CR.RXE). IMSC additionally unmasks the RX interrupt at the PL011
     * itself -- real GIC delivery of PL011's SPI still needs the separate
     * root-gated interrupt_create + interrupt_bind below; this register is
     * what makes the device actually assert that line in the first place.
     */
    inline void configure_uart() noexcept {
        auto* control = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + control_offset);
        auto* imsc = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + imsc_offset);
        *control = cr_uarten | cr_txe | cr_rxe;
        *imsc = imsc_rxim;
    }

    inline void putc(char value) noexcept {
        auto* data = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + data_offset);
        auto* flags = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + flag_offset);
        while ((*flags & transmit_fifo_full) != 0U) {
        }
        *data = static_cast<sys::u32>(static_cast<sys::u8>(value));
    }

    [[nodiscard]] inline bool try_getc(sys::u8& value) noexcept {
        auto* data = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + data_offset);
        auto* flags = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + flag_offset);
        if ((*flags & receive_fifo_empty) != 0U)
            return false;
        value = static_cast<sys::u8>(*data);
        return true;
    }

    /*
     * RX ring buffer, drained from real hardware only when the bound IRQ
     * notification signals (see drain_rx() below) -- not re-checked
     * unconditionally on every loop wakeup the way console-server's old
     * single-pending-byte scheme was. Sized past the PL011's own 16-byte
     * hardware FIFO: one interrupt can signal several queued bytes at
     * once, and a 1-byte holding cell would drop the rest.
     */
    inline constexpr sys::usize_t rx_ring_capacity = 64U;
    sys::u8 rx_ring[rx_ring_capacity]{};
    sys::usize_t rx_head = 0U;
    sys::usize_t rx_count = 0U;

    inline void rx_push(sys::u8 value) noexcept {
        if (rx_count == rx_ring_capacity)
            return; // full: drop rather than overwrite unread bytes
        rx_ring[(rx_head + rx_count) % rx_ring_capacity] = value;
        ++rx_count;
    }

    [[nodiscard]] inline bool rx_pop(sys::u8& value) noexcept {
        if (rx_count == 0U)
            return false;
        value = rx_ring[rx_head];
        rx_head = (rx_head + 1U) % rx_ring_capacity;
        --rx_count;
        return true;
    }

    /*
     * Drains real hardware into the ring buffer and re-arms the interrupt.
     * Level-triggered (confirmed against this platform's real device tree:
     * pl011@9000000's interrupts property is <0 1 4>, GIC SPI 1 = INTID
     * 33, flags 4 = level-high) -- the PL011 keeps asserting its line
     * while unread data sits at or above its FIFO trigger level, so this
     * must actually empty the FIFO before interrupt_ack re-unmasks/
     * re-routes it, or the same condition would immediately refire (which
     * is self-correcting, not unsafe, but pointless work).
     */
    inline void drain_rx() noexcept {
        sys::u8 value = 0U;
        while (try_getc(value))
            rx_push(value);
        auto* icr = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + icr_offset);
        *icr = icr_rxic;
        (void)sys::control(sys::abi::v1::control_operation::interrupt_ack, irq_selector);
    }

    [[nodiscard]] inline bool map_uart() noexcept {
        const sys::word_t read_write = static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
                                       static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::device, sys::abi::v1::memory_shareability::non_shareable);
        return native::retry([&] {
            return native::ok(sys::control(sys::abi::v1::control_operation::map_frame,
                                           self_space_selector, uart_frame_selector,
                                           uart_scratch_address, read_write, attrs));
        });
    }

    // Same ordering hazard console-server's map_uart() already documented:
    // root can only mint the IRQ capability into this process's cspace
    // after process_create returns, so this process can genuinely start
    // and reach here first. Bounded retry, not a one-shot attempt.
    [[nodiscard]] inline bool bind_irq() noexcept {
        if (!native::ok(sys::control(sys::abi::v1::control_operation::notification_create,
                                     irq_notification_selector)))
            return false;
        return native::retry([&] {
            return native::ok(sys::control(sys::abi::v1::control_operation::interrupt_bind,
                                           irq_selector, irq_notification_selector));
        });
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    if (!map_uart() || !bind_irq()) {
        native::signal_failure();
        return 1;
    }
    configure_uart();

    native::signal_ready(sys::abi::v1::serial_service_ready_badge);

    for (;;) {
        sys::word_t signaled = 0U;
        const sys::word_t polled = sys::control_result1(
            signaled, sys::abi::v1::control_operation::notification_poll,
            irq_notification_selector);
        if (polled == static_cast<sys::word_t>(sys::error_t::success) && signaled != 0U)
            drain_rx();

        // Bounded timeout, not an indefinite blocking receive, so this loop
        // keeps checking the IRQ notification between requests -- same
        // idiom as root_graph.hh's drain_fault_reports() and
        // domain-manager's forward_device_irqs(); this codebase has no
        // blocking-wait syscall for notifications.
        const auto request =
            sys::ipc_receive(service_endpoint, sys::abi::v1::encode_timeout(1U));
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<sys::abi::v1::serial_operation>(request.message0);
        sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        sys::word_t result1 = 0U;
        if (operation == sys::abi::v1::serial_operation::write) {
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
        } else if (operation == sys::abi::v1::serial_operation::write_byte) {
            putc(static_cast<char>(request.message1 & 0xffU));
            result0 = static_cast<sys::word_t>(sys::error_t::success);
        } else if (operation == sys::abi::v1::serial_operation::read_byte) {
            sys::u8 value = 0U;
            const bool available = rx_pop(value);
            result0 = available ? 1U : 0U;
            result1 = available ? value : 0U;
        }
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
