#pragma once

#include <sys/types.hh>

namespace sys::platform::memory
{
    inline constexpr paddr_t ram_base = 0x40000000ULL;
    inline constexpr psize_t ram_size =
        static_cast<psize_t>(CONFIG_QEMU_RAM_MB) * 1024ULL * 1024ULL;
    inline constexpr paddr_t uart_base = 0x09000000ULL;
    inline constexpr psize_t uart_size = 0x1000ULL;
    inline constexpr paddr_t firmware_dtb_probe = ram_base + 0x08000000ULL;

    /*
     * virtio-mmio transport window. Confirmed against this platform's real
     * device tree (dumpdtb of the same `virt,gic-version=3,virtualization=on`
     * machine tools/run/run.sh boots): 32 `virtio,mmio` nodes starting at
     * virtio_mmio@a000000, each `reg` 0x200 bytes, consecutive -- so one
     * 4 KiB grant page covers transports 0..7. Their interrupts are
     * <0 16 1>..<0 47 1>: GIC SPI 16..47 = INTID 48..79, flags 1 =
     * edge-rising (NOT level-high like pl011@9000000's <0 1 4>, which
     * matters for how a driver acknowledges them).
     *
     * Exactly one page of that window is grantable, and it is the LAST one,
     * not the first: QEMU populates these transports downward from the top.
     * Confirmed with `info qtree` on this machine -- a single
     * `-device virtio-blk-device` lands on virtio-mmio-bus.31, i.e.
     * 0x0a000000 + 31 * 0x200 = 0x0a003e00. So the page that actually
     * carries devices is 0x0a003000, covering transports 24..31; transports
     * 0..23 sit empty until many devices are attached. Granting the first
     * page instead reads back a window of valid-magic/device_id==0 slots and
     * finds nothing, which is precisely what the driver's probe reported
     * before this was pinned down.
     *
     * Keeping it to a single page keeps device_frame_create()'s exclusivity
     * check meaningful.
     */
    inline constexpr paddr_t virtio_mmio_base = 0x0a000000ULL;
    inline constexpr psize_t virtio_mmio_stride = 0x200ULL;
    inline constexpr u32 virtio_mmio_transport_count = 32U;
    inline constexpr psize_t virtio_mmio_grant_size = 0x1000ULL;
    inline constexpr u32 virtio_mmio_page_transports =
        static_cast<u32>(virtio_mmio_grant_size / virtio_mmio_stride);
    inline constexpr paddr_t virtio_mmio_grant_base =
        virtio_mmio_base +
        (virtio_mmio_transport_count - virtio_mmio_page_transports) * virtio_mmio_stride;
    // Index of the first transport inside the granted page (24), so a driver
    // can turn a page-relative slot into its real INTID.
    inline constexpr u32 virtio_mmio_grant_first_transport =
        virtio_mmio_transport_count - virtio_mmio_page_transports;
    inline constexpr irq_id_t virtio_mmio_first_irq = 48U;

    static_assert(virtio_mmio_grant_base == 0x0a003000ULL);

    [[nodiscard]] inline constexpr bool valid_device_page(paddr_t address) noexcept {
        return address == uart_base || address == virtio_mmio_grant_base;
    }

    static_assert(CONFIG_QEMU_RAM_MB >= 128);
} // namespace sys::platform::memory
