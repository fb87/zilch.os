#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/native.hh>
#include <sys/thread.hh>
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
    inline constexpr sys::word_t uart_scratch_address = 0x20053000U;

    /*
     * The RX half runs on its own thread receiving on its own endpoint, and
     * owns everything below that touches the RX ring: the bound interrupt
     * notification, drain_rx(), and both read operations. The main thread
     * serves only write/write_byte.
     *
     * That split is not about throughput -- it is what makes read_byte_wait's
     * deferred reply safe. A deferred reply is held in the serving thread's
     * single reply slot, which the kernel overwrites on the next incoming
     * call to that thread, so a write arriving while a read was parked used
     * to strand the reader in blocked_reply for good (and stdin with it).
     * Must match root_graph.hh's serial_rx_role /
     * serial_rx_child_endpoint_selector / serial_rx_thread_selector /
     * serial_rx_space_selector, and see serial_rx_endpoint's comment there
     * for the capability-graph invariants the deferral relies on.
     */
    inline constexpr sys::word_t rx_role = 0x10bU;
    inline constexpr sys::capability_id_t rx_endpoint = 23U;
    inline constexpr sys::capability_id_t rx_thread_selector = 30U;
    inline constexpr sys::capability_id_t rx_space_selector = 31U;

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

    /*
     * Bring-up diagnostics written STRAIGHT to the device, with no IPC.
     *
     * Every other process reports through this driver, so this driver is
     * the one that cannot: a failure here takes the console down with it,
     * and then root, the block driver and console-server all block forever
     * inside their own report calls. That produced the worst failure shape
     * in this system -- a boot that stops dead with no output at all, or
     * mid-character partway through another process's line, and no way to
     * tell which of them was at fault. Since this process owns the PL011
     * outright once map_uart() succeeds, it can just write the bytes.
     *
     * Only for bring-up failures, not for the serving path: it bypasses
     * the endpoint that serializes writes, so concurrent output from a
     * healthy system would interleave mid-character.
     */
    inline void report(const char* text) noexcept {
        for (const char* cursor = text; *cursor != '\0'; ++cursor)
            putc(*cursor);
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
     * re-routes it.
     *
     * ORDER IS LOAD-BEARING: clear RXIC *before* each drain pass, and only
     * stop once a pass ends with the FIFO genuinely empty.
     *
     * The obvious order -- drain, then clear -- silently loses the RX
     * interrupt for the rest of the system's uptime, and did. The PL011
     * raises RX when the FIFO level *reaches* its trigger, not while it
     * sits at or above it, so the assertion is an edge on the way up. A
     * byte landing in the window between the final try_getc() (which saw
     * RXFE, so returned false) and the ICR write leaves the FIFO
     * non-empty while ICR wipes the latch that byte had just set. The
     * level can then never re-cross the trigger from below -- the FIFO
     * never returns to empty, because nothing will interrupt to drain it
     * -- so RX is dead permanently, taking every console reader with it:
     * the interactive shell and, through the same driver, any hosted
     * guest's console.
     *
     * Clearing first inverts that: anything arriving during or after a
     * drain pass keeps its latch, and the loop re-checks rather than
     * trusting the pass it just did. Reproduced and fixed against
     * tools/verification/guest_input_burst.sh, which needs a sustained
     * burst to hit the window at all -- a short one almost never does,
     * which is why this survived earlier testing.
     */
    inline void drain_rx() noexcept {
        auto* icr = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + icr_offset);
        auto* flags = reinterpret_cast<volatile sys::u32*>(uart_scratch_address + flag_offset);
        sys::u8 value = 0U;
        /*
         * NEVER clear the RX latch while the FIFO still holds data.
         *
         * The rule comes from how the interrupt is regenerated. In the
         * PL011 model this platform runs against, the receive interrupt is
         * raised on exactly one event -- the receive count going from empty
         * to one -- and is cleared either by draining back to empty or by
         * any write to ICR, which drops the latch unconditionally no matter
         * what the FIFO contains. There is no receive-timeout interrupt to
         * fall back on; it is simply not implemented there.
         *
         * So clearing the latch with a byte still buffered is
         * unrecoverable. The line goes low, the byte stays put, and because
         * the device is then full it refuses further input -- so the
         * empty-to-one transition that would re-raise the interrupt can
         * never happen again. Console input stops permanently with every
         * component healthy: that is checklist 0156, and it is why no
         * amount of guest-side instrumentation ever found it. The old order
         * cleared ICR at the top of each pass, before reading anything, and
         * lost the race whenever the drain that followed failed to observe
         * the byte the clear had just disarmed.
         *
         * Draining to empty FIRST and only then clearing inverts that. The
         * post-clear re-check is what keeps the previously documented
         * hazard closed: a byte landing between the emptiness test and the
         * clear has its own latch wiped, so the loop must look again rather
         * than trust the pass it just did. Either way the invariant on exit
         * is the one that matters -- the FIFO is empty, so a cleared latch
         * costs nothing and the next byte re-raises the interrupt.
         */
        for (;;) {
            while (try_getc(value))
                rx_push(value);
            if ((*flags & receive_fifo_empty) == 0U)
                continue; // more arrived while draining
            *icr = icr_rxic;
            if ((*flags & receive_fifo_empty) != 0U)
                break; // still empty after the clear: nothing was disarmed
        }
        (void)sys::control(sys::abi::v1::control_operation::interrupt_ack, irq_selector);
    }

    /*
     * Takes the target address space explicitly because this driver has two
     * threads and thread_create gives each thread its OWN address space --
     * only the cspace is shared (see create_user_thread() in
     * thread/scheduler.hh: "Each sibling owns its own address_space"). The
     * UART frame therefore has to be mapped once per thread that touches
     * the device registers, at the same scratch address in both, or the RX
     * thread faults on its first drain. memory::map() allows that: it keeps
     * a mapping record per (space, address) pair and only rejects a repeat
     * of the SAME pair, so the second map is a genuine second mapping of the
     * one device frame rather than a conflict.
     */
    [[nodiscard]] inline bool map_uart(sys::capability_id_t space_selector) noexcept {
        const sys::word_t read_write =
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::device, sys::abi::v1::memory_shareability::non_shareable);
        return native::retry([&] {
            return native::ok(sys::control(sys::abi::v1::control_operation::map_frame, space_selector,
                                           uart_frame_selector, uart_scratch_address, read_write,
                                           attrs));
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

    /*
     * The RX thread (spawned by main() below under rx_role). Binds the
     * interrupt notification to ITSELF -- notification_bind is per-thread,
     * so this must run here and not on the main thread -- which is what lets
     * the ipc_receive() below wake on RX as well as on a request.
     *
     * Deferring read_byte_wait's reply is safe on this thread and only on
     * this thread: root mints rx_endpoint to console-server alone, and
     * console-server serves it from a single thread that blocks in ipc_call,
     * so at most one RX request is ever outstanding and the one reply slot
     * is always enough. Nothing else can reach this thread to overwrite it.
     */
    [[noreturn]] inline void rx_main() noexcept {
        /*
         * This thread's own view of the UART, and its own binding of the
         * interrupt notification -- both are per-thread and neither is
         * inherited from the main thread that created it (see map_uart()'s
         * comment for the address space, and notification_bind's own
         * per-thread semantics for the notification).
         *
         * A failure here would leave RX permanently dead while TX kept
         * working, which looks like a hung console rather than a failed
         * driver. Report it to root and exit the thread rather than falling
         * into the loop below, which would touch an unmapped UART and fault
         * -- repeatedly, and against a driver that is deliberately not
         * restart-covered (see USR-024), so the faults would never resolve.
         *
         * rx_space_selector, not native::own_space: slot 3 still names the
         * MAIN thread's address space, which is where map_uart() already
         * mapped the device for the write path. create_user_thread()
         * installs this thread's own space capability at the selector it was
         * created with, and that is the one to map into here.
         */
        if (!map_uart(rx_space_selector)) {
            // No mapping means no way to say so on the device itself.
            native::signal_failure();
            sys::thread_exit(1U, native::root_notification, native::failure_badge);
        }
        if (!native::ok(sys::control(sys::abi::v1::control_operation::notification_bind,
                                     irq_notification_selector))) {
            report("serial: rx notification bind failed\r\n");
            native::signal_failure();
            sys::thread_exit(1U, native::root_notification, native::failure_badge);
        }

        // Set once a read_byte_wait request finds the ring empty: the
        // caller's reply capability stays implicitly stashed by the kernel
        // (current.reply) across the next ipc_receive() below, so this only
        // needs to remember *that* a reply is owed, to answer it once
        // drain_rx() has bytes. Never set for plain read_byte, which always
        // replies immediately -- see its own ABI comment.
        bool reply_pending = false;

        for (;;) {
            const auto request = sys::ipc_receive(rx_endpoint);
            if (request.status == static_cast<sys::word_t>(sys::error_t::notification_signal)) {
                drain_rx();
                if (reply_pending) {
                    sys::u8 value = 0U;
                    if (rx_pop(value)) {
                        (void)sys::ipc_reply(1U, static_cast<sys::word_t>(value), 0U, 0U);
                        reply_pending = false;
                    }
                }
                continue;
            }
            if (request.status != static_cast<sys::word_t>(sys::error_t::success))
                continue;
            const auto operation = static_cast<sys::abi::v1::serial_operation>(request.message0);
            sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
            sys::word_t result1 = 0U;
            if (operation == sys::abi::v1::serial_operation::read_byte) {
                sys::u8 value = 0U;
                const bool available = rx_pop(value);
                result0 = available ? 1U : 0U;
                result1 = available ? value : 0U;
            } else if (operation == sys::abi::v1::serial_operation::read_byte_wait) {
                sys::u8 value = 0U;
                if (rx_pop(value)) {
                    result0 = 1U;
                    result1 = value;
                } else {
                    reply_pending = true;
                    continue; // answered from the drain path above
                }
            }
            (void)sys::ipc_reply(result0, result1, 0U, 0U);
        }
    }

    /*
     * Same ordering hazard console-server's own stdin-thread spawn already
     * documents: root can only mint rx_endpoint into this cspace after
     * process_create returns, so this process can genuinely reach here
     * first. Spawning the RX thread before the mint lands would leave it
     * spinning on failed capability resolution instead of blocking. A
     * timed-out probe means the endpoint resolves and is merely idle, which
     * is the success signal; not_found/denied mean the mint has not landed.
     */
    [[nodiscard]] inline bool await_rx_endpoint() noexcept {
        constexpr sys::word_t attempts = 100000U;
        for (sys::word_t attempt = 0U; attempt < attempts; ++attempt) {
            const auto probe = sys::ipc_receive(rx_endpoint, sys::abi::v1::encode_timeout(1U));
            if (probe.status != static_cast<sys::word_t>(sys::error_t::not_found) &&
                probe.status != static_cast<sys::word_t>(sys::error_t::denied))
                return true;
        }
        return false;
    }
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    if (role == rx_role)
        rx_main(); // noreturn

    /*
     * map_uart() is the one failure this process genuinely cannot report:
     * without the mapping there is no device to write to. Everything after
     * it reports through report() directly, since a bring-up failure here
     * silently takes the whole system's console with it.
     */
    if (!map_uart(self_space_selector)) {
        native::signal_failure();
        return 1;
    }
    configure_uart();
    if (!bind_irq()) {
        report("serial: irq bind failed\r\n");
        native::signal_failure();
        return 1;
    }

    // CPU 3 deliberately: not root's CPU 0, and not this driver's own (root
    // process_creates serial-driver onto CPU 2).
    if (!await_rx_endpoint()) {
        report("serial: rx endpoint never minted\r\n");
        native::signal_failure();
        return 1;
    }
    if (sys::control(sys::abi::v1::control_operation::thread_create, 3U, rx_role,
                     rx_thread_selector,
                     rx_space_selector) != static_cast<sys::word_t>(sys::error_t::success)) {
        report("serial: rx thread create failed\r\n");
        native::signal_failure();
        return 1;
    }

    native::signal_ready(sys::abi::v1::serial_service_ready_badge);

    for (;;) {
        const auto request = sys::ipc_receive(service_endpoint);
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
        }
        // read_byte/read_byte_wait are NOT served here -- they belong to the
        // RX thread's own endpoint (see rx_main()). Reaching this endpoint
        // with one means the caller aimed at the wrong capability, which the
        // invalid_argument default already reports.
        if (sys::ipc_reply(result0, result1, 0U, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
