#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/memory.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/control_plane.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/serial.hh>
#include <abi/sys/v1/virtio.hh>

namespace
{
    namespace mmio = sys::abi::v1::virtio_mmio;
    namespace abi = sys::abi::v1;

    namespace native = sys::native;

    // Well-known slots and the ready/failure protocol now come from the
    // native personality rather than being restated here.
    inline constexpr sys::capability_id_t service_endpoint = native::service_endpoint;

    /*
     * root creates the one live virtio-mmio device frame (root-gated, and
     * exclusivity-checked against the same physical page) and mints it plus
     * a capability to serial-driver's service endpoint into these slots
     * before this process ever runs -- see root_graph.hh's
     * create_block_resources()/mint_block_resources(). The DMA frame is this
     * driver's own: it creates and allocates it itself, which is also why it
     * holds the control right frame_physical_address requires.
     */
    inline constexpr sys::capability_id_t mmio_frame_selector = 20U;
    inline constexpr sys::capability_id_t irq_selector = 21U;
    inline constexpr sys::capability_id_t irq_notification_selector = 22U;
    inline constexpr sys::capability_id_t serial_endpoint_selector = 23U;
    inline constexpr sys::capability_id_t dma_frame_selector = 24U;
    /*
     * Payload buffer, a different frame from the ring page above so a client
     * holding it cannot reach the virtqueue. Root mints it with control as
     * well as read/write so this driver can resolve its physical address.
     */
    inline constexpr sys::capability_id_t shared_frame_selector = 25U;
    inline constexpr sys::word_t shared_scratch_address = 0x2003c000U;

    /*
     * The IRQ capability root minted covers INTID 79 = transport 31, the
     * transport QEMU populates first. If a differently-populated machine
     * ever puts the block device elsewhere, the capability would be for the
     * wrong line, so the driver checks rather than assumes -- and falls back
     * to polling, saying so, instead of waiting on a line that never fires.
     */
    inline constexpr sys::u32 expected_irq = 79U;
    inline constexpr sys::capability_id_t self_space_selector = native::own_space;
    inline constexpr sys::word_t mmio_scratch_address = 0x2003e000U;
    inline constexpr sys::word_t dma_scratch_address = 0x2003d000U;

    /*
     * One 4 KiB grant page covers 8 transports at 0x200 stride. That page is
     * the LAST of the window (transports 24..31), because QEMU populates
     * these transports downward from the top -- a single attached device
     * lands on transport 31. Confirmed with `info qtree`; see
     * platform/qemu_arm64_virt/memory.hh's virtio_mmio_grant_base.
     *
     * grant_first_transport turns a page-relative index into the real
     * transport number, which is what the INTID is derived from.
     */
    inline constexpr sys::u32 transports_per_page = 8U;
    inline constexpr sys::u32 grant_first_transport = 24U;
    inline constexpr sys::uintptr_t transport_stride = 0x200U;
    inline constexpr sys::u32 virtio_first_irq = 48U;

    [[nodiscard]] inline sys::u32 transport_irq(sys::u32 page_index) noexcept {
        return virtio_first_irq + grant_first_transport + page_index;
    }

    /*
     * Split-virtqueue layout inside the single DMA page. Offsets are chosen
     * so each structure meets its own alignment (descriptors 16, available
     * ring 2, used ring 4) with room to spare; a queue of 8 needs 128/24/72
     * bytes respectively, so everything including the request header, status
     * byte, and one sector of data fits well inside 4 KiB.
     */
    inline constexpr sys::u32 queue_size = 8U;
    inline constexpr sys::uintptr_t desc_offset = 0x000U;
    inline constexpr sys::uintptr_t avail_offset = 0x080U;
    inline constexpr sys::uintptr_t used_offset = 0x100U;
    inline constexpr sys::uintptr_t header_offset = 0x200U;
    inline constexpr sys::uintptr_t status_offset = 0x210U;

    static_assert(status_offset < 0x1000U);

    // Payload lives at the base of the separate shared frame, so the whole
    // sector is reachable by a client without exposing the rings above.
    inline constexpr sys::uintptr_t data_offset = 0x000U;

    sys::u32 block_transport = transports_per_page; // == none found
    sys::word_t dma_physical = 0U;
    sys::word_t shared_physical = 0U;
    sys::u64 device_capacity = 0U;
    bool interrupt_bound = false;
    // Counted so a boot can distinguish "the interrupt path works" from
    // "we silently fell back to spinning and nobody noticed".
    sys::u64 interrupt_signals = 0U;

    /* ---- MMIO accessors ---- */

    [[nodiscard]] inline volatile sys::u32* reg(sys::u32 transport, sys::uintptr_t offset) noexcept {
        return reinterpret_cast<volatile sys::u32*>(
            mmio_scratch_address + static_cast<sys::uintptr_t>(transport) * transport_stride +
            offset);
    }

    [[nodiscard]] inline sys::u32 read_reg(sys::u32 transport, sys::uintptr_t offset) noexcept {
        return *reg(transport, offset);
    }

    inline void write_reg(sys::u32 transport, sys::uintptr_t offset, sys::u32 value) noexcept {
        *reg(transport, offset) = value;
    }

    /* ---- DMA accessors ---- */

    template <typename T>
    [[nodiscard]] inline volatile T* dma(sys::uintptr_t offset) noexcept {
        return reinterpret_cast<volatile T*>(dma_scratch_address + offset);
    }

    template <typename T>
    [[nodiscard]] inline volatile T* payload(sys::uintptr_t offset) noexcept {
        return reinterpret_cast<volatile T*>(shared_scratch_address + offset);
    }

    /*
     * The virtio-mmio nodes carry `dma-coherent` (confirmed in this
     * platform's device tree), and valid_attributes() requires a normal RAM
     * frame to be mapped normal + inner-shareable, so the rings are ordinary
     * cacheable memory that the device sees coherently. That makes ordering
     * -- not cache maintenance -- the only requirement: a full system
     * barrier before publishing an index the device may read, and after
     * observing one it wrote.
     */
    inline void barrier() noexcept {
        __asm__ volatile("dsb sy" ::: "memory");
    }

    inline void write_desc(sys::u32 index, sys::u64 address, sys::u32 length, sys::u16 flags,
                           sys::u16 next) noexcept {
        const sys::uintptr_t base = desc_offset + static_cast<sys::uintptr_t>(index) * 16U;
        *dma<sys::u64>(base) = address;
        *dma<sys::u32>(base + 8U) = length;
        *dma<sys::u16>(base + 12U) = flags;
        *dma<sys::u16>(base + 14U) = next;
    }

    /* ---- diagnostics over serial-driver, so bring-up is actually visible ---- */

    inline void emit_text(const char* value) noexcept {
        native::text::write(serial_endpoint_selector, value);
    }

    inline void emit_hex(sys::u64 value) noexcept {
        native::text::hex(serial_endpoint_selector, value);
    }

    /* ---- capability setup ---- */

    // Same ordering hazard the serial driver's map_uart() documents: root
    // can only mint into this cspace after process_create returns, so this
    // process can genuinely start and reach here first. Bounded retry.
    [[nodiscard]] inline bool map_mmio() noexcept {
        const sys::word_t read_write = static_cast<sys::word_t>(abi::CapabilityRight::read) |
                                       static_cast<sys::word_t>(abi::CapabilityRight::write);
        const sys::word_t attrs = abi::encode_mapping_attributes(
            abi::memory_type::device, abi::memory_shareability::non_shareable);
        return native::retry([&] {
            return native::ok(sys::control(abi::control_operation::map_frame, self_space_selector,
                                           mmio_frame_selector, mmio_scratch_address, read_write,
                                           attrs));
        });
    }

    /*
     * Same ordering hazard as the mapping above -- root mints the IRQ
     * capability only after process_create returns, so this process can
     * reach here first. Bounded retry, matching serial-driver's bind_irq().
     */
    [[nodiscard]] inline bool bind_irq() noexcept {
        if (!native::ok(sys::control(abi::control_operation::notification_create,
                                     irq_notification_selector)))
            return false;
        return native::retry([&] {
            return native::ok(sys::control(abi::control_operation::interrupt_bind, irq_selector,
                                           irq_notification_selector));
        });
    }

    /*
     * The DMA page is this driver's own frame, not a delegated one: it
     * creates and allocates it, so it holds the control right that
     * frame_physical_address requires. A normal RAM frame must be mapped
     * normal + inner-shareable (memory::valid_attributes()), which is also
     * exactly what a coherent DMA buffer wants.
     */
    [[nodiscard]] inline bool setup_dma() noexcept {
        const sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
        sys::word_t status =
            sys::control(abi::control_operation::frame_create, 0U, dma_frame_selector);
        if (status != success) {
            emit_text("virtio: frame_create failed ");
            emit_hex(status);
            emit_text("\r\n");
            return false;
        }
        // No frame_allocate here: create_frame already calls assign_frame,
        // so the frame comes back allocated and physically backed. Calling
        // frame_allocate on top of it correctly fails with busy.
        const sys::error_t physical =
            sys::frame_physical_address(dma_frame_selector, dma_physical);
        if (physical != sys::error_t::success) {
            emit_text("virtio: frame_physical_address failed ");
            emit_hex(static_cast<sys::u64>(static_cast<sys::word_t>(physical)));
            emit_text("\r\n");
            return false;
        }
        emit_text("virtio: dma page at ");
        emit_hex(dma_physical);
        emit_text("\r\n");

        // Payload frame: root created and minted it (with control, so this
        // call is permitted). Separate from the ring page above so a client
        // holding it cannot reach the virtqueue.
        const sys::error_t shared =
            sys::frame_physical_address(shared_frame_selector, shared_physical);
        if (shared != sys::error_t::success) {
            emit_text("virtio: shared frame_physical_address failed ");
            emit_hex(static_cast<sys::u64>(static_cast<sys::word_t>(shared)));
            emit_text("\r\n");
            return false;
        }
        const sys::word_t read_write = static_cast<sys::word_t>(abi::CapabilityRight::read) |
                                       static_cast<sys::word_t>(abi::CapabilityRight::write);
        const sys::word_t attrs = abi::encode_mapping_attributes(
            abi::memory_type::normal, abi::memory_shareability::inner_shareable);
        status = sys::control(abi::control_operation::map_frame, self_space_selector,
                              dma_frame_selector, dma_scratch_address, read_write, attrs);
        if (status != success) {
            emit_text("virtio: dma map_frame failed ");
            emit_hex(status);
            emit_text("\r\n");
            return false;
        }
        status = sys::control(abi::control_operation::map_frame, self_space_selector,
                              shared_frame_selector, shared_scratch_address, read_write, attrs);
        if (status != success) {
            emit_text("virtio: shared map_frame failed ");
            emit_hex(status);
            emit_text("\r\n");
            return false;
        }
        for (sys::uintptr_t offset = 0U; offset < 0x1000U; offset += 8U) {
            *dma<sys::u64>(offset) = 0U;
            *payload<sys::u64>(offset) = 0U;
        }
        return true;
    }

    /*
     * Which transport slot QEMU plugs a given -device into is not answerable
     * from the device tree (every slot is present and reads valid magic
     * regardless; only device_id distinguishes an empty one), so scan rather
     * than hardcode.
     */
    inline void probe_transports() noexcept {
        for (sys::u32 index = 0U; index < transports_per_page; ++index) {
            if (read_reg(index, mmio::magic_value) != mmio::magic)
                continue;
            const sys::u32 identifier = read_reg(index, mmio::device_id);
            if (identifier == mmio::device_id_none)
                continue;
            emit_text("virtio:  transport ");
            emit_hex(grant_first_transport + index);
            emit_text(" id=");
            emit_hex(identifier);
            emit_text(" version=");
            emit_hex(read_reg(index, mmio::version));
            emit_text("\r\n");
            if (identifier == mmio::device_id_block && block_transport == transports_per_page)
                block_transport = index;
        }
    }

    /*
     * Modern (MMIO version 2) initialisation, VIRTIO 1.2 section 3.1.1.
     * run.sh passes -global virtio-mmio.force-legacy=false to get this
     * transport version; legacy would need the QueuePFN/GuestPageSize path
     * instead, which this driver deliberately does not implement.
     */
    [[nodiscard]] inline bool initialize_device() noexcept {
        const sys::u32 t = block_transport;
        if (read_reg(t, mmio::version) != mmio::version_modern) {
            emit_text("virtio: not a modern transport\r\n");
            return false;
        }

        write_reg(t, mmio::status, 0U); // reset
        write_reg(t, mmio::status, mmio::status_acknowledge);
        write_reg(t, mmio::status, mmio::status_acknowledge | mmio::status_driver);

        // Accept only VIRTIO_F_VERSION_1 (feature bit 32 = bit 0 of word 1).
        write_reg(t, mmio::device_features_sel, abi::virtio_f_version_1_word);
        const sys::u32 high_features = read_reg(t, mmio::device_features);
        if ((high_features & abi::virtio_f_version_1_bit) == 0U) {
            emit_text("virtio: device lacks VERSION_1\r\n");
            return false;
        }
        write_reg(t, mmio::driver_features_sel, 0U);
        write_reg(t, mmio::driver_features, 0U);
        write_reg(t, mmio::driver_features_sel, abi::virtio_f_version_1_word);
        write_reg(t, mmio::driver_features, abi::virtio_f_version_1_bit);

        write_reg(t, mmio::status,
                  mmio::status_acknowledge | mmio::status_driver | mmio::status_features_ok);
        if ((read_reg(t, mmio::status) & mmio::status_features_ok) == 0U) {
            emit_text("virtio: FEATURES_OK rejected\r\n");
            return false;
        }

        write_reg(t, mmio::queue_sel, 0U);
        const sys::u32 max_queue = read_reg(t, mmio::queue_num_max);
        if (max_queue < queue_size) {
            emit_text("virtio: queue too small\r\n");
            return false;
        }
        write_reg(t, mmio::queue_num, queue_size);
        const sys::u64 desc_physical = static_cast<sys::u64>(dma_physical) + desc_offset;
        const sys::u64 avail_physical = static_cast<sys::u64>(dma_physical) + avail_offset;
        const sys::u64 used_physical = static_cast<sys::u64>(dma_physical) + used_offset;
        write_reg(t, mmio::queue_desc_low, static_cast<sys::u32>(desc_physical));
        write_reg(t, mmio::queue_desc_high, static_cast<sys::u32>(desc_physical >> 32U));
        write_reg(t, mmio::queue_driver_low, static_cast<sys::u32>(avail_physical));
        write_reg(t, mmio::queue_driver_high, static_cast<sys::u32>(avail_physical >> 32U));
        write_reg(t, mmio::queue_device_low, static_cast<sys::u32>(used_physical));
        write_reg(t, mmio::queue_device_high, static_cast<sys::u32>(used_physical >> 32U));
        barrier();
        write_reg(t, mmio::queue_ready, 1U);

        write_reg(t, mmio::status,
                  mmio::status_acknowledge | mmio::status_driver | mmio::status_features_ok |
                      mmio::status_driver_ok);

        // virtio-blk config space starts with capacity in 512-byte sectors.
        device_capacity = static_cast<sys::u64>(read_reg(t, mmio::config)) |
                          (static_cast<sys::u64>(read_reg(t, mmio::config + 4U)) << 32U);
        return true;
    }

    /*
     * One request = three chained descriptors (header, data, status), the
     * layout every virtio-blk request uses. Submits, rings the doorbell, and
     * polls the used ring: this driver holds no interrupt capability yet
     * (the IRQ number depends on the transport slot, which is only known
     * after the probe above), so completion is polled rather than waited on.
     */
    sys::u16 avail_index = 0U;
    sys::u16 used_seen = 0U;

    inline constexpr sys::word_t completion_attempts = 10000000U;

    /*
     * error_t has no io_error, and the two ways a request fails are
     * genuinely different, so distinguish them here rather than collapsing
     * both into one status the caller cannot act on: the device rejecting a
     * request is permanent, the poll budget running out is not.
     */
    enum class request_result { completed, device_error, no_completion };

    [[nodiscard]] inline sys::word_t status_for(request_result value) noexcept {
        switch (value) {
            case request_result::completed:
                return static_cast<sys::word_t>(sys::error_t::success);
            case request_result::device_error:
                return static_cast<sys::word_t>(sys::error_t::busy);
            default:
                return static_cast<sys::word_t>(sys::error_t::timed_out);
        }
    }

    [[nodiscard]] inline request_result submit(sys::u32 type, sys::u64 sector) noexcept {
        *dma<sys::u32>(header_offset) = type;
        *dma<sys::u32>(header_offset + 4U) = 0U;
        *dma<sys::u64>(header_offset + 8U) = sector;
        *dma<sys::u8>(status_offset) = 0xffU; // poison, device overwrites

        const sys::u64 base = static_cast<sys::u64>(dma_physical);
        const sys::u16 data_flags = static_cast<sys::u16>(
            abi::virtq_desc_next |
            (type == abi::block_request_in ? abi::virtq_desc_write : 0U));
        write_desc(0U, base + header_offset, 16U, abi::virtq_desc_next, 1U);
        write_desc(1U, static_cast<sys::u64>(shared_physical) + data_offset,
                   abi::block_sector_size, data_flags, 2U);
        write_desc(2U, base + status_offset, 1U, abi::virtq_desc_write, 0U);

        // Publish the head into the available ring, then its index. The
        // barrier between the two is what keeps the device from seeing an
        // index that points at a ring slot it has not yet been shown.
        *dma<sys::u16>(avail_offset + 4U + (avail_index % queue_size) * 2U) = 0U;
        barrier();
        avail_index = static_cast<sys::u16>(avail_index + 1U);
        *dma<sys::u16>(avail_offset + 2U) = avail_index;
        barrier();

        write_reg(block_transport, mmio::queue_notify, 0U);

        for (sys::word_t attempt = 0U; attempt < completion_attempts; ++attempt) {
            /*
             * Drain the bound notification first. The kernel masks the line
             * on delivery and interrupt_ack re-arms it, so this must run even
             * when the used ring is what actually tells us the request
             * finished -- otherwise the line stays masked and no later
             * request would ever see an interrupt.
             *
             * This codebase has no blocking wait for a notification (see
             * serial-driver's loop and root_graph.hh's drain_fault_reports()),
             * so the loop still spins; the interrupt's value here is keeping
             * the device's edge-triggered line correctly acknowledged, and
             * being the hook an asynchronous completion path would use.
             */
            if (interrupt_bound) {
                sys::word_t signaled = 0U;
                const sys::word_t polled =
                    sys::control_result1(signaled, abi::control_operation::notification_poll,
                                         irq_notification_selector);
                if (polled == static_cast<sys::word_t>(sys::error_t::success) && signaled != 0U) {
                    ++interrupt_signals;
                    const sys::u32 pending = read_reg(block_transport, mmio::interrupt_status);
                    if (pending != 0U)
                        write_reg(block_transport, mmio::interrupt_ack, pending);
                    (void)sys::control(abi::control_operation::interrupt_ack, irq_selector);
                }
            }

            const sys::u16 used = *dma<sys::u16>(used_offset + 2U);
            if (used != used_seen) {
                barrier();
                used_seen = used;
                // Edge-triggered line (see main.md): clear whatever the device
                // still has raised, so the next request starts clean.
                const sys::u32 pending = read_reg(block_transport, mmio::interrupt_status);
                if (pending != 0U)
                    write_reg(block_transport, mmio::interrupt_ack, pending);
                return *dma<sys::u8>(status_offset) == abi::block_status_ok
                           ? request_result::completed
                           : request_result::device_error;
            }
        }
        return request_result::no_completion;
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    if (!map_mmio()) {
        native::signal_failure();
        return 1;
    }
    probe_transports();

    bool ready = false;
    if (block_transport != transports_per_page) {
        emit_text("virtio: block device at transport ");
        emit_hex(grant_first_transport + block_transport);
        emit_text(" irq=");
        emit_hex(transport_irq(block_transport));
        emit_text("\r\n");
        // DMA setup only matters once a device is actually present, and
        // running it after the probe means a failure here is reported with
        // the discovery context already on the console.
        // Bind before DRIVER_OK so no completion can be raised on an
        // unbound line. Only when the discovered transport is the one the
        // minted capability actually covers.
        if (transport_irq(block_transport) == expected_irq) {
            interrupt_bound = bind_irq();
            emit_text(interrupt_bound ? "virtio: irq bound\r\n"
                                      : "virtio: irq bind failed, polling\r\n");
        } else {
            emit_text("virtio: transport irq differs from granted capability, polling\r\n");
        }
        ready = setup_dma() && initialize_device();
        if (ready) {
            emit_text("virtio: queue ready, capacity ");
            emit_hex(device_capacity);
            emit_text(" sectors\r\n");
            /*
             * Self-check across the WHOLE sector, not just its first word: a
             * single-word check would pass even if the descriptor length were
             * wrong, or if only the leading bytes were actually transferred.
             * Each 8-byte slot gets a distinct value derived from its offset,
             * so a misaligned or short transfer cannot coincidentally match.
             */
            const sys::u64 probe_sector = device_capacity != 0U ? device_capacity - 1U : 0U;
            for (sys::uintptr_t offset = 0U; offset < abi::block_sector_size; offset += 8U)
                *payload<sys::u64>(data_offset + offset) = 0x7a696c6368626c6bULL ^ offset;
            const bool wrote =
                submit(abi::block_request_out, probe_sector) == request_result::completed;
            for (sys::uintptr_t offset = 0U; offset < abi::block_sector_size; offset += 8U)
                *payload<sys::u64>(data_offset + offset) = 0U;
            const bool read_back =
                wrote && submit(abi::block_request_in, probe_sector) == request_result::completed;
            sys::uintptr_t verified = 0U;
            while (verified < abi::block_sector_size &&
                   *payload<sys::u64>(data_offset + verified) == (0x7a696c6368626c6bULL ^ verified))
                verified += 8U;
            const sys::u64 value = *payload<sys::u64>(data_offset);
            const bool intact = read_back && verified == abi::block_sector_size;
            emit_text("virtio: sector round trip ");
            emit_text(intact ? "PASS" : "FAIL");
            emit_text(" bytes=");
            emit_hex(verified);
            if (!intact) {
                emit_text(" first_bad=");
                emit_hex(value);
            }
            if (interrupt_bound) {
                /*
                 * Whether a given request's interrupt is *observed* by the
                 * loop is racy: QEMU services the doorbell write inline, so
                 * the used ring has often already moved by the time the
                 * first poll runs, and the request returns before the
                 * notification is seen. The count therefore varies run to
                 * run (0 or 1 across the two requests below) and is not a
                 * sound boot assertion on its own.
                 *
                 * What IS sound: by this point two requests have completed,
                 * so if the line were dead neither the running count nor a
                 * final drain would show anything. Checking both reports
                 * "live" without depending on which request won the race.
                 */
                sys::word_t signaled = 0U;
                (void)sys::control_result1(signaled, abi::control_operation::notification_poll,
                                           irq_notification_selector);
                emit_text(interrupt_signals != 0U || signaled != 0U ? " irq=live" : " irq=silent");
            } else {
                emit_text(" irq=polling");
            }
            emit_text("\r\n");
        }
    } else {
        emit_text("virtio: no block device present\r\n");
    }

    native::signal_ready(abi::block_service_ready_badge);

    for (;;) {
        // Bounded timeout rather than an indefinite blocking receive, same
        // idiom as the serial driver's loop and root_graph.hh's
        // drain_fault_reports().
        const auto request = sys::ipc_receive(service_endpoint, abi::encode_timeout(1U));
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<abi::block_operation>(request.message0);
        sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        sys::word_t result1 = 0U;
        sys::word_t result2 = 0U;
        sys::word_t result3 = 0U;

        if (operation == abi::block_operation::probe) {
            const sys::u32 index = static_cast<sys::u32>(request.message1);
            if (index < transports_per_page) {
                result0 = static_cast<sys::word_t>(sys::error_t::success);
                result1 = read_reg(index, mmio::device_id);
                result2 = read_reg(index, mmio::version);
            }
        } else if (operation == abi::block_operation::info) {
            if (ready) {
                result0 = static_cast<sys::word_t>(sys::error_t::success);
                result1 = static_cast<sys::word_t>(device_capacity);
                result2 = abi::block_sector_size;
            } else {
                result0 = static_cast<sys::word_t>(sys::error_t::not_found);
            }
        } else if (operation == abi::block_operation::read) {
            if (!ready) {
                result0 = static_cast<sys::word_t>(sys::error_t::not_found);
            } else if (request.message1 >= device_capacity) {
                result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
            } else {
                const request_result outcome = submit(abi::block_request_in, request.message1);
                result0 = status_for(outcome);
                if (outcome == request_result::completed) {
                    result1 = *payload<sys::u64>(data_offset);
                    result2 = *payload<sys::u64>(data_offset + 8U);
                    result3 = *payload<sys::u64>(data_offset + 16U);
                }
            }
        } else if (operation == abi::block_operation::write) {
            if (!ready) {
                result0 = static_cast<sys::word_t>(sys::error_t::not_found);
            } else if (request.message1 >= device_capacity) {
                result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
            } else {
                *payload<sys::u64>(data_offset) = request.message2;
                *payload<sys::u64>(data_offset + 8U) = request.message3;
                for (sys::uintptr_t offset = 16U; offset < abi::block_sector_size; offset += 8U)
                    *payload<sys::u64>(data_offset + offset) = 0U;
                result0 = status_for(submit(abi::block_request_out, request.message1));
            }
        }

        if (sys::ipc_reply(result0, result1, result2, result3) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
