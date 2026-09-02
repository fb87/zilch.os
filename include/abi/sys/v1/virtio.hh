#pragma once

#include <sys/types.hh>

namespace sys::abi::v1
{
    /*
     * virtio-mmio transport register layout (VIRTIO 1.2, section 4.2.2).
     * Offsets are from a transport's own base; on this platform the
     * transports are 0x200 apart inside one grantable page -- see
     * platform/qemu_arm64_virt's virtio_mmio_base/virtio_mmio_stride, which
     * is where the *addresses* live. Only the layout is ABI; userspace has
     * no access to platform:: headers, same reason the serial driver
     * restates the PL011 register offsets locally.
     */
    namespace virtio_mmio
    {
        inline constexpr uintptr_t magic_value = 0x000U;
        inline constexpr uintptr_t version = 0x004U;
        inline constexpr uintptr_t device_id = 0x008U;
        inline constexpr uintptr_t vendor_id = 0x00cU;
        inline constexpr uintptr_t device_features = 0x010U;
        inline constexpr uintptr_t device_features_sel = 0x014U;
        inline constexpr uintptr_t driver_features = 0x020U;
        inline constexpr uintptr_t driver_features_sel = 0x024U;
        inline constexpr uintptr_t queue_sel = 0x030U;
        inline constexpr uintptr_t queue_num_max = 0x034U;
        inline constexpr uintptr_t queue_num = 0x038U;
        inline constexpr uintptr_t queue_ready = 0x044U;
        inline constexpr uintptr_t queue_notify = 0x050U;
        inline constexpr uintptr_t interrupt_status = 0x060U;
        inline constexpr uintptr_t interrupt_ack = 0x064U;
        inline constexpr uintptr_t status = 0x070U;
        inline constexpr uintptr_t queue_desc_low = 0x080U;
        inline constexpr uintptr_t queue_desc_high = 0x084U;
        inline constexpr uintptr_t queue_driver_low = 0x090U;
        inline constexpr uintptr_t queue_driver_high = 0x094U;
        inline constexpr uintptr_t queue_device_low = 0x0a0U;
        inline constexpr uintptr_t queue_device_high = 0x0a4U;
        inline constexpr uintptr_t config = 0x100U;

        // "virt" little-endian, present on every transport slot whether or
        // not a device is plugged into it. An unpopulated slot still reads
        // this magic and a valid version -- device_id 0 is what marks it
        // empty, so a probe must check device_id, not magic alone.
        inline constexpr u32 magic = 0x74726976U;
        inline constexpr u32 version_modern = 2U;
        inline constexpr u32 version_legacy = 1U;

        inline constexpr u32 device_id_none = 0U;
        inline constexpr u32 device_id_block = 2U;

        // Status bits (VIRTIO 1.2, section 2.1).
        inline constexpr u32 status_acknowledge = 1U;
        inline constexpr u32 status_driver = 2U;
        inline constexpr u32 status_driver_ok = 4U;
        inline constexpr u32 status_features_ok = 8U;
        inline constexpr u32 status_needs_reset = 64U;
        inline constexpr u32 status_failed = 128U;
    } // namespace virtio_mmio

    /*
     * Private wire protocol between clients and the virtio block driver
     * (src/user/drivers/virtio), same non-control-plane-role convention as
     * serial_operation: the block driver doesn't fit the fixed 5-slot
     * control_plane_role enum, so it defines its own small ABI enum.
     */
    enum class block_operation : word_t {
        // No payload. Replies message0 = error_t::success and
        // message1 = capacity in 512-byte sectors, message2 = sector size.
        info = 0U,
        // Probe diagnostics: message1 = transport index within the granted
        // page. Replies message0 = error_t::success, message1 = that
        // transport's device_id, message2 = its version.
        probe = 1U,
        /*
         * Read one sector. message1 = sector index. On success the sector
         * lands in the driver's bounce buffer; the first 4 words are echoed
         * back in message1..message3 (24 bytes) so a caller can verify a
         * round trip without a shared mapping. A capability-granted shared
         * data frame is the next step for bulk transfer.
         */
        read = 2U,
        /*
         * Write one sector. message1 = sector index, message2/message3 =
         * the first 16 bytes of payload, zero-filled to the sector size.
         * Deliberately narrow for the same reason as read.
         */
        write = 3U,
    };

    inline constexpr word_t block_sector_size = 512U;

    /*
     * virtio-blk request header (VIRTIO 1.2, section 5.2.6). Followed by the
     * data buffer and a one-byte status, as three chained descriptors.
     */
    inline constexpr u32 block_request_in = 0U;  // device -> driver (read)
    inline constexpr u32 block_request_out = 1U; // driver -> device (write)

    inline constexpr u8 block_status_ok = 0U;
    inline constexpr u8 block_status_io_error = 1U;
    inline constexpr u8 block_status_unsupported = 2U;

    // Split-virtqueue descriptor flags (VIRTIO 1.2, section 2.7.5).
    inline constexpr u16 virtq_desc_next = 1U;
    inline constexpr u16 virtq_desc_write = 2U;

    // VIRTIO_F_VERSION_1 is feature bit 32, i.e. bit 0 of feature word 1.
    inline constexpr u32 virtio_f_version_1_word = 1U;
    inline constexpr u32 virtio_f_version_1_bit = 1U << 0U;
} // namespace sys::abi::v1
