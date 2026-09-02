#include <sys/control.hh>
#include <sys/ipc.hh>
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

    inline constexpr sys::capability_id_t root_notification = 14U;
    inline constexpr sys::capability_id_t service_endpoint = 11U;
    inline constexpr sys::word_t failure_badge = 1U << 15U;

    /*
     * root creates the one live virtio-mmio device frame (root-gated, and
     * exclusivity-checked against the same physical page) and mints it plus
     * a capability to serial-driver's service endpoint into these slots
     * before this process ever runs -- see root_graph.hh's
     * create_block_resources()/mint_block_resources(). This process only
     * ever maps/uses capabilities that are already there.
     */
    inline constexpr sys::capability_id_t mmio_frame_selector = 20U;
    inline constexpr sys::capability_id_t serial_endpoint_selector = 23U;
    inline constexpr sys::capability_id_t self_space_selector = 3U;
    inline constexpr sys::word_t mmio_scratch_address = 0x2003e000U;

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

    [[nodiscard]] inline volatile sys::u32* reg(sys::u32 transport, sys::uintptr_t offset) noexcept {
        return reinterpret_cast<volatile sys::u32*>(
            mmio_scratch_address + static_cast<sys::uintptr_t>(transport) * transport_stride +
            offset);
    }

    [[nodiscard]] inline sys::u32 read_reg(sys::u32 transport, sys::uintptr_t offset) noexcept {
        return *reg(transport, offset);
    }

    /* ---- diagnostics over serial-driver, so a probe is actually visible ---- */

    inline void emit(char value) noexcept {
        (void)sys::ipc_call(serial_endpoint_selector,
                            static_cast<sys::word_t>(sys::abi::v1::serial_operation::write_byte),
                            static_cast<sys::word_t>(static_cast<sys::u8>(value)));
    }

    inline void emit_text(const char* text) noexcept {
        for (const char* cursor = text; *cursor != '\0'; ++cursor)
            emit(*cursor);
    }

    inline void emit_hex(sys::u32 value) noexcept {
        emit('0');
        emit('x');
        bool leading = true;
        for (int shift = 28; shift >= 0; shift -= 4) {
            const sys::u32 digit = (value >> static_cast<sys::u32>(shift)) & 0xfU;
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
        const sys::word_t read_write =
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read) |
            static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::write);
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::device, sys::abi::v1::memory_shareability::non_shareable);
        for (sys::word_t attempt = 0U; attempt < map_attempts; ++attempt) {
            if (sys::control(sys::abi::v1::control_operation::map_frame, self_space_selector,
                             mmio_frame_selector, mmio_scratch_address, read_write, attrs) ==
                static_cast<sys::word_t>(sys::error_t::success))
                return true;
        }
        return false;
    }

    /*
     * Which transport slot QEMU plugs a given -device into is not something
     * the device tree can answer (all 32 nodes are present whether or not a
     * device is attached: an empty slot still reads the "virt" magic and a
     * valid version, only device_id distinguishes it). So scan rather than
     * hardcode a slot, and report what was actually found -- picking the
     * IRQ to request depends on this answer, which is exactly why no
     * interrupt capability is created for this driver yet.
     */
    sys::u32 block_transport = transports_per_page; // == none found

    inline void probe_transports() noexcept {
        emit_text("virtio: scanning ");
        emit_hex(transports_per_page);
        emit_text(" transports\r\n");
        for (sys::u32 index = 0U; index < transports_per_page; ++index) {
            const sys::u32 magic = read_reg(index, mmio::magic_value);
            if (magic != mmio::magic) {
                emit_text("virtio:  slot ");
                emit_hex(index);
                emit_text(" bad magic ");
                emit_hex(magic);
                emit_text("\r\n");
                continue;
            }
            const sys::u32 identifier = read_reg(index, mmio::device_id);
            if (identifier == mmio::device_id_none)
                continue;
            emit_text("virtio:  transport ");
            emit_hex(grant_first_transport + index);
            emit_text(" id=");
            emit_hex(identifier);
            emit_text(" version=");
            emit_hex(read_reg(index, mmio::version));
            emit_text(" vendor=");
            emit_hex(read_reg(index, mmio::vendor_id));
            emit_text("\r\n");
            if (identifier == mmio::device_id_block && block_transport == transports_per_page)
                block_transport = index;
        }
        if (block_transport == transports_per_page) {
            emit_text("virtio: no block device present\r\n");
            return;
        }
        emit_text("virtio: block device at transport ");
        emit_hex(grant_first_transport + block_transport);
        emit_text(" irq=");
        // INTID = 48 + transport, from the device tree's <0 16 1>..<0 47 1>.
        emit_hex(transport_irq(block_transport));
        emit_text("\r\n");
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    if (!map_mmio()) {
        (void)sys::control(sys::abi::v1::control_operation::notification_signal, root_notification,
                           failure_badge);
        return 1;
    }
    probe_transports();

    const sys::word_t status = sys::control(sys::abi::v1::control_operation::notification_signal,
                                            root_notification,
                                            sys::abi::v1::block_service_ready_badge);
    if (status != static_cast<sys::word_t>(sys::error_t::success))
        return 2;

    for (;;) {
        // Bounded timeout rather than an indefinite blocking receive, same
        // idiom as the serial driver's loop and root_graph.hh's
        // drain_fault_reports().
        const auto request =
            sys::ipc_receive(service_endpoint, sys::abi::v1::encode_timeout(1U));
        if (request.status != static_cast<sys::word_t>(sys::error_t::success))
            continue;
        const auto operation = static_cast<sys::abi::v1::block_operation>(request.message0);
        sys::word_t result0 = static_cast<sys::word_t>(sys::error_t::invalid_argument);
        sys::word_t result1 = 0U;
        sys::word_t result2 = 0U;
        if (operation == sys::abi::v1::block_operation::probe) {
            const sys::u32 index = static_cast<sys::u32>(request.message1);
            if (index < transports_per_page) {
                result0 = static_cast<sys::word_t>(sys::error_t::success);
                result1 = read_reg(index, mmio::device_id);
                result2 = read_reg(index, mmio::version);
            }
        } else if (operation == sys::abi::v1::block_operation::info) {
            if (block_transport != transports_per_page) {
                // Capacity comes from the device's config space once the
                // queue is live; until then report the transport slot so a
                // client can at least confirm discovery succeeded.
                result0 = static_cast<sys::word_t>(sys::error_t::success);
                result1 = block_transport;
                result2 = sys::abi::v1::block_sector_size;
            } else {
                result0 = static_cast<sys::word_t>(sys::error_t::not_found);
            }
        }
        if (sys::ipc_reply(result0, result1, result2, 0U) !=
            static_cast<sys::word_t>(sys::error_t::success))
            return 3;
    }
}
