#pragma once

#include <sys/kernel/hypervisor/stage2.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::hypervisor_stage2
{
    /*
     * Stage-2 table construction, on the positive path.
     *
     * Certification already covers the rejection paths -- overlap, W^X,
     * alignment, unmapping something absent -- and those are the ones that
     * fail loudly. What had no coverage is the descriptor a successful
     * mapping actually installs, and that is the side where a mistake is
     * silent: a guest whose device memory was mapped cacheable, or whose
     * non-executable page lost its XN bit, works perfectly until it does
     * not, and never produces an error anyone can trace back here.
     *
     * So this asserts the exact bits, against the architectural meaning
     * rather than against whatever the function currently emits.
     */
    [[nodiscard]] inline error_t run_descriptors() noexcept
    {
        using namespace sys::kernel::hypervisor;
        constexpr paddr_t address = 0x0000'0000'4321'2000ULL;
        constexpr auto read = static_cast<u32>(stage2_permission::read);
        constexpr auto write = static_cast<u32>(stage2_permission::write);
        constexpr auto execute = static_cast<u32>(stage2_permission::execute);
        constexpr auto device = static_cast<u32>(stage2_permission::device);

        // Read-only normal memory, not executable.
        const u64 ro = stage2_leaf_descriptor(address, read);
        if ((ro & stage2_address_mask) != address)
            return error_t::invalid_argument; // output frame must be preserved exactly
        if ((ro & stage2_descriptor_table) != stage2_descriptor_table ||
            (ro & stage2_descriptor_access) == 0U)
            return error_t::invalid_argument; // a leaf must be valid and accessed
        if ((ro & stage2_descriptor_read_write) != stage2_descriptor_read_only)
            return error_t::invalid_argument;
        if ((ro & stage2_descriptor_execute_never) != stage2_descriptor_execute_never)
            return error_t::invalid_argument; // no execute permission means XN

        // Write permission must widen S2AP, and nothing else about it.
        const u64 rw = stage2_leaf_descriptor(address, read | write);
        if ((rw & stage2_descriptor_read_write) != stage2_descriptor_read_write)
            return error_t::invalid_argument;
        if ((rw ^ ro) != (stage2_descriptor_read_write ^ stage2_descriptor_read_only))
            return error_t::invalid_argument; // write changed a bit it had no business changing

        // Execute permission must clear XN, and nothing else.
        const u64 rx = stage2_leaf_descriptor(address, read | execute);
        if ((rx & stage2_descriptor_execute_never) != 0U)
            return error_t::invalid_argument;
        if ((rx ^ ro) != stage2_descriptor_execute_never)
            return error_t::invalid_argument;

        /*
         * Device memory must NOT carry the normal-memory attributes or
         * inner-shareable. Getting this wrong maps MMIO as cacheable
         * write-back, where a guest's register writes can sit in a cache
         * line instead of reaching the device -- which presents as a driver
         * that intermittently does nothing, not as a fault.
         */
        const u64 mmio = stage2_leaf_descriptor(address, read | write | device);
        if ((mmio & stage2_descriptor_normal_memory) != 0U)
            return error_t::invalid_argument;
        if ((mmio & stage2_descriptor_inner_shareable) != 0U)
            return error_t::invalid_argument;
        if ((mmio & stage2_descriptor_read_write) != stage2_descriptor_read_write)
            return error_t::invalid_argument;

        // And normal memory must carry both.
        if ((rw & stage2_descriptor_normal_memory) != stage2_descriptor_normal_memory ||
            (rw & stage2_descriptor_inner_shareable) != stage2_descriptor_inner_shareable)
            return error_t::invalid_argument;

        pr_info("[TEST] name=stage2_leaf_descriptor result=PASS frame_preserved=1 "
                "s2ap=1 xn=1 device_uncached=1\n");
        return error_t::success;
    }

    /*
     * Accessed/dirty tracking query and clear.
     *
     * The generation is what lets a consumer tell "nothing happened since I
     * last asked" from "I missed a whole cycle", so the rule that it never
     * takes the value zero is load-bearing: zero is the never-queried
     * value, and a wrap that produced it would silently look like a fresh
     * mapping.
     */
    [[nodiscard]] inline error_t run_tracking() noexcept
    {
        using namespace sys::kernel::hypervisor;
        static virtual_machine_t vm{};
        vm.lock = 0U;
        for (auto& mapping : vm.mappings)
            mapping = {};

        auto& mapping = vm.mappings[0];
        mapping.valid = true;
        mapping.ipa = 0x40000000ULL;
        mapping.size = 0x2000ULL;
        mapping.accessed = true;
        mapping.dirty = true;
        mapping.tracking_generation = 1U;

        u64 flags = 0U;
        // An IPA outside every mapping is not_found, not a zeroed success.
        if (stage2_tracking(vm, 0x3fffffffULL, false, flags) != error_t::not_found ||
            stage2_tracking(vm, 0x40002000ULL, false, flags) != error_t::not_found)
            return error_t::invalid_argument;

        // Query without clear reports the flags and leaves them alone.
        if (stage2_tracking(vm, 0x40001000ULL, false, flags) != error_t::success)
            return error_t::invalid_argument;
        if ((flags & 1U) == 0U || (flags & 2U) == 0U || (flags >> 32U) != 1U)
            return error_t::invalid_argument;
        if (!mapping.accessed || !mapping.dirty || mapping.tracking_generation != 1U)
            return error_t::invalid_argument; // a read must not mutate

        // Clear reports what was observed, then resets and advances.
        if (stage2_tracking(vm, 0x40000000ULL, true, flags) != error_t::success)
            return error_t::invalid_argument;
        if ((flags & 3U) != 3U || (flags >> 32U) != 1U)
            return error_t::invalid_argument; // reports the pre-clear state
        if (mapping.accessed || mapping.dirty || mapping.tracking_generation != 2U)
            return error_t::invalid_argument;

        // Generation must skip zero on wrap: zero means never queried.
        mapping.tracking_generation = ~static_cast<u32>(0U);
        if (stage2_tracking(vm, 0x40000000ULL, true, flags) != error_t::success)
            return error_t::invalid_argument;
        if (mapping.tracking_generation == 0U)
            return error_t::invalid_argument;

        pr_info("[TEST] name=stage2_tracking result=PASS bounds=1 read_is_pure=1 "
                "clear_reports_prior=1 generation_skips_zero=1\n");
        return error_t::success;
    }

    [[nodiscard]] inline error_t run() noexcept
    {
        const error_t descriptors = run_descriptors();
        if (descriptors != error_t::success)
            return descriptors;
        return run_tracking();
    }
} // namespace sys::kernel::tests::hypervisor_stage2
