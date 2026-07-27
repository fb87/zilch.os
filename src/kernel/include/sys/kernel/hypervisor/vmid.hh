#pragma once

#include <sys/kernel/hypervisor/object.hh>

namespace sys::kernel::hypervisor
{
    inline volatile u32 next_vmid = 1U;
    inline volatile u64 vmid_in_use = 1ULL; // VMID 0 is reserved.

    [[nodiscard]] inline error_t allocate_vmid(u16& vmid) noexcept {
        for (u32 candidate = 1U; candidate < maximum_vmids; ++candidate) {
            const u64 bit = 1ULL << candidate;
            const u64 previous = __atomic_fetch_or(&vmid_in_use, bit, __ATOMIC_ACQ_REL);
            if ((previous & bit) == 0U) {
                vmid = static_cast<u16>(candidate);
                return error_t::success;
            }
        }
        return error_t::no_memory;
    }

    [[nodiscard]] inline error_t release_vmid(u16 vmid) noexcept {
        if (vmid == 0U || vmid >= maximum_vmids)
            return error_t::invalid_argument;
        arch::hypervisor::invalidate_stage2(vmid);
        __atomic_fetch_and(&vmid_in_use, ~(1ULL << vmid), __ATOMIC_ACQ_REL);
        return error_t::success;
    }
    [[nodiscard]] inline bool aligned(u64 value) noexcept {
        return (value & (page_size - 1U)) == 0U;
    }
    [[nodiscard]] inline bool contains_wx(u32 permissions) noexcept {
        return (permissions & static_cast<u32>(stage2_permission::write)) != 0U &&
               (permissions & static_cast<u32>(stage2_permission::execute)) != 0U;
    }
} // namespace sys::kernel::hypervisor
