#include <sys/control.hh>
#include <sys/ipc.hh>
#include <sys/memory.hh>
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

    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    inline constexpr sys::word_t failure_badge = 1U << 15U;

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
    inline constexpr sys::capability_id_t serial_endpoint_selector = 23U;
    inline constexpr sys::capability_id_t dma_frame_selector = 24U;
    inline constexpr sys::capability_id_t self_space_selector = 3U;
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
    inline constexpr sys::uintptr_t data_offset = 0x400U;

    static_assert(data_offset + abi::block_sector_size <= 0x1000U);

    sys::u32 block_transport = transports_per_page; // == none found
    sys::word_t dma_physical = 0U;
    sys::u64 device_capacity = 0U;

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

    inline void emit(char value) noexcept {
        (void)sys::ipc_call(serial_endpoint_selector,
                            static_cast<sys::word_t>(abi::serial_operation::write_byte),
                            static_cast<sys::word_t>(static_cast<sys::u8>(value)));
    }

    inline void emit_text(const char* text) noexcept {
        for (const char* cursor = text; *cursor != '\0'; ++cursor)
            emit(*cursor);
    }

    inline void emit_hex(sys::u64 value) noexcept {
        emit('0');
        emit('x');
        bool leading = true;
        for (int shift = 60; shift >= 0; shift -= 4) {
            const sys::u64 digit = (value >> static_cast<sys::u64>(shift)) & 0xfULL;
            if (digit == 0U && leading && shift != 0)
                continue;
            leading = false;
            emit(static_cast<char>(digit < 10U ? '0' + digit : 'a' + (digit - 10U)));
        }
    }

    /* ---- capability setup ---- */

    // Same ordering hazard the serial driver's map_uart() documents: root
    // can only mint into this cspace after process_create returns, so this
    // process can genuinely start and reach here first. Bounded retry.
    inline constexpr sys::word_t map_attempts = 100000U;

    [[nodiscard]] inline bool map_mmio() noexcept {
        const sys::word_t read_write = static_cast<sys::word_t>(abi::CapabilityRight::read) |
                                       static_cast<sys::word_t>(abi::CapabilityRight::write);
        const sys::word_t attrs = abi::encode_mapping_attributes(
            abi::memory_type::device, abi::memory_shareability::non_shareable);
        for (sys::word_t attempt = 0U; attempt < map_attempts; ++attempt) {
            if (sys::control(abi::control_operation::map_frame, self_space_selector,
                             mmio_frame_selector, mmio_scratch_address, read_write, attrs) ==
                static_cast<sys::word_t>(sys::error_t::success))
                return true;
        }
        return false;
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
        for (sys::uintptr_t offset = 0U; offset < 0x1000U; offset += 8U)
            *dma<sys::u64>(offset) = 0U;
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
        write_desc(1U, base + data_offset, abi::block_sector_size, data_flags, 2U);
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
            const sys::u16 used = *dma<sys::u16>(used_offset + 2U);
            if (used != used_seen) {
                barrier();
                used_seen = used;
                // Edge-triggered line (see main.md): acknowledge whatever the
                // device raised so a later interrupt-driven path starts clean.
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
        (void)sys::control(abi::control_operation::notification_signal, root_notification,
                           failure_badge);
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
        ready = setup_dma() && initialize_device();
        if (ready) {
            emit_text("virtio: queue ready, capacity ");
            emit_hex(device_capacity);
            emit_text(" sectors\r\n");
            // Self-check: write a recognisable pattern to the last sector
            // and read it back, so a boot proves the DMA path end to end
            // rather than only proving the device was discovered.
            const sys::u64 probe_sector = device_capacity != 0U ? device_capacity - 1U : 0U;
            *dma<sys::u64>(data_offset) = 0x7a696c6368626c6bULL; // "zilchblk"
            for (sys::uintptr_t offset = 8U; offset < abi::block_sector_size; offset += 8U)
                *dma<sys::u64>(data_offset + offset) = 0U;
            const bool wrote =
                submit(abi::block_request_out, probe_sector) == request_result::completed;
            *dma<sys::u64>(data_offset) = 0U;
            const bool read_back =
                wrote && submit(abi::block_request_in, probe_sector) == request_result::completed;
            const sys::u64 value = *dma<sys::u64>(data_offset);
            emit_text("virtio: sector round trip ");
            emit_text(read_back && value == 0x7a696c6368626c6bULL ? "PASS" : "FAIL");
            emit_text(" value=");
            emit_hex(value);
            emit_text("\r\n");
        }
    } else {
        emit_text("virtio: no block device present\r\n");
    }

    const sys::word_t status =
        sys::control(abi::control_operation::notification_signal, root_notification,
                     abi::block_service_ready_badge);
    if (status != static_cast<sys::word_t>(sys::error_t::success))
        return 2;

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
                    result1 = *dma<sys::u64>(data_offset);
                    result2 = *dma<sys::u64>(data_offset + 8U);
                    result3 = *dma<sys::u64>(data_offset + 16U);
                }
            }
        } else if (operation == abi::block_operation::write) {
            if (!ready) {
                result0 = static_cast<sys::word_t>(sys::error_t::not_found);
            } else if (request.message1 >= device_capacity) {
                result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
            } else {
                *dma<sys::u64>(data_offset) = request.message2;
                *dma<sys::u64>(data_offset + 8U) = request.message3;
                for (sys::uintptr_t offset = 16U; offset < abi::block_sector_size; offset += 8U)
                    *dma<sys::u64>(data_offset + offset) = 0U;
                result0 = status_for(submit(abi::block_request_out, request.message1));
            }
        }

        if (sys::ipc_reply(result0, result1, result2, result3) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
