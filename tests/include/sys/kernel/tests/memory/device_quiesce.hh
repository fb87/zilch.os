#pragma once

#include <sys/kernel/memory/manager.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/task/task.hh>

namespace sys::kernel::tests::device_quiesce
{
    /*
     * A revoked device frame is quiesced, not merely unmapped (DEV-004).
     *
     * Unmapping stops the old owner from NAMING the device. It does not stop
     * the device. A virtio transport left with DRIVER_OK and a programmed
     * virtqueue still holds the DMA addresses its driver gave it, and
     * reclaim_task_memory() has just returned those pages to the free pool --
     * so one more completed request lands in whatever is allocated there
     * next. Every incoming driver does reset its own device during bring-up,
     * which is why this has never been observed, but that is the new owner
     * protecting itself rather than the system guaranteeing a clean device,
     * and it does nothing for the window in between or for a device left
     * live with no owner at all.
     *
     * The write loop is driven against a scratch RAM page rather than real
     * MMIO. Pointing it at the actual PL011 or virtio window would mean
     * masking the console's interrupts or resetting the block device the
     * driver is currently using, to prove something about an address
     * calculation; RAM is identity-mapped in the kernel, so the same code
     * path runs and the result can be read back and checked exactly.
     */
    alignas(4096) inline u8 scratch_page[4096]{};

    [[nodiscard]] inline error_t run(task::task& root) noexcept
    {
        constexpr u32 offset = 0x070U; // virtio Status
        constexpr u32 stride = 0x200U; // one transport
        constexpr u32 count = 8U;      // every transport in a granted page
        /*
         * A fully negotiated virtio status: ACKNOWLEDGE|DRIVER|FEATURES_OK|
         * DRIVER_OK. This is the state that leaks (DEV-017) -- a transport
         * left here is one the device considers owned and driveable, with
         * its virtqueue live. Using the real bit pattern rather than an
         * arbitrary sentinel so the "before" state is the one that actually
         * occurs when a driver dies mid-service.
         */
        constexpr u32 negotiated = 1U | 2U | 8U | 4U;
        constexpr u32 quiesced = 0U;

        const auto word_at = [](u32 byte_offset) noexcept -> volatile u32& {
            return *reinterpret_cast<volatile u32*>(&scratch_page[byte_offset]);
        };

        /*
         * Seed every target first. Reading back zero is only evidence if
         * something other than zero was there to begin with -- the page is
         * statically zero-initialised, so without this the test would pass
         * just as well if the write loop never ran at all.
         */
        for (u32 index = 0U; index < count; ++index)
            word_at(offset + index * stride) = negotiated;

        memory::frame probe{};
        probe.physical_address = reinterpret_cast<paddr_t>(&scratch_page[0]);
        probe.device = true;
        probe.mapping_count = 0U;
        probe.quiesce_declared = true;
        probe.quiesce_offset = offset;
        probe.quiesce_value = quiesced;
        probe.quiesce_count = count;
        probe.quiesce_stride = stride;

        memory::release_frame_mappings(probe);

        for (u32 index = 0U; index < count; ++index) {
            if (word_at(offset + index * stride) != quiesced)
                return error_t::invalid_argument; // a transport was left live
        }

        /*
         * And nothing outside the declared targets may be touched. The
         * kernel performs these writes with full privilege against an
         * offset supplied by userspace, so "wrote the right words" is only
         * half the property -- writing extra ones would be a silent
         * corruption primitive.
         */
        for (u32 index = 0U; index < count; ++index) {
            const u32 neighbour = offset + index * stride + sizeof(u32);
            if (neighbour + sizeof(u32) > sizeof(scratch_page))
                continue;
            if (word_at(neighbour) != 0U)
                return error_t::invalid_argument;
        }

        /*
         * The bounds checks are the security-relevant half: without them a
         * declaration is an arbitrary-kernel-write primitive for anyone
         * permitted to create device frames. Each rejection must happen at
         * declaration time, before the frame exists -- validating at
         * revocation would be too late, because by then the owning driver
         * may be gone and nobody is left to fail.
         */
        constexpr capability_id_t probe_selector = 21U;
        constexpr paddr_t uart_page = 0x09000000ULL;
        struct {
            u32 offset;
            u32 count;
            u32 stride;
        } const rejected[] = {
            {0x39U, 1U, 0U},        // misaligned offset
            {0x38U, 1U, 0x201U},    // misaligned stride
            {0x38U, 0U, 0U},        // declared but empty
            {0x1000U, 1U, 0U},      // starts past the page
            {0xffcU, 2U, 0x200U},   // first write fits, last does not
            {0x70U, 0xffffU, 0x200U}, // count * stride would wrap a narrower type
        };

        for (const auto& bad : rejected) {
            if (memory::create_device_frame(root, probe_selector, uart_page, true, bad.offset,
                                            0U, bad.count, bad.stride) !=
                error_t::invalid_argument)
                return error_t::invalid_argument;
        }

        pr_info("[TEST] name=device_revocation_quiesce result=PASS transports=%u stride=0x%x "
                "next_owner_sees_reset=1 neighbours_untouched=1 rejected=%u\n",
                count, stride, static_cast<u32>(sizeof(rejected) / sizeof(rejected[0])));
        return error_t::success;
    }
} // namespace sys::kernel::tests::device_quiesce
