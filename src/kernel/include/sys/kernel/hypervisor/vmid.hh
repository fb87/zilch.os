#pragma once

#include <sys/kernel/hypervisor/object.hh>
#include <sys/kernel/lock/order.hh>

namespace sys::kernel::hypervisor
{
    inline u64 vmid_in_use = 1ULL; // VMID 0 is reserved.
    inline volatile u32 vmid_lock{};
    inline u32 vmid_generation{1U};
    inline u32 vmid_allocations{};
    inline u64 vmid_rollovers{};

    inline void lock_vmid_allocator() noexcept {
        while (__atomic_exchange_n(&vmid_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
        lock_order::acquired(lock_order::rank::translation_identifier, &vmid_lock);
    }

    inline void unlock_vmid_allocator() noexcept {
        lock_order::released(lock_order::rank::translation_identifier, &vmid_lock);
        __atomic_store_n(&vmid_lock, 0U, __ATOMIC_RELEASE);
    }

    inline void rollover_vmids_locked() noexcept {
        arch::hypervisor::invalidate_stage2(0U);
        ++vmid_generation;
        if (vmid_generation == 0U)
            vmid_generation = 1U;
        vmid_in_use = 1ULL;
        vmid_allocations = 0U;
        ++vmid_rollovers;
    }

    [[nodiscard]] inline error_t allocate_vmid(u16& vmid, u32& generation) noexcept {
        lock_vmid_allocator();
        if (vmid_allocations >= maximum_vmids - 1U)
            rollover_vmids_locked();
        for (u32 candidate = 1U; candidate < maximum_vmids; ++candidate) {
            const u64 bit = 1ULL << candidate;
            if ((vmid_in_use & bit) == 0U) {
                vmid_in_use |= bit;
                ++vmid_allocations;
                vmid = static_cast<u16>(candidate);
                generation = vmid_generation;
                unlock_vmid_allocator();
                return error_t::success;
            }
        }
        rollover_vmids_locked();
        vmid_in_use |= 1ULL << 1U;
        ++vmid_allocations;
        vmid = 1U;
        generation = vmid_generation;
        unlock_vmid_allocator();
        return error_t::success;
    }

    [[nodiscard]] inline error_t allocate_vmid(u16& vmid) noexcept {
        u32 generation{};
        return allocate_vmid(vmid, generation);
    }

    [[nodiscard]] inline error_t release_vmid(u16 vmid, u32 generation = 0U) noexcept {
        if (vmid == 0U || vmid >= maximum_vmids)
            return error_t::invalid_argument;
        arch::hypervisor::invalidate_stage2(vmid);
        lock_vmid_allocator();
        if (generation == 0U || generation == vmid_generation)
            vmid_in_use &= ~(1ULL << vmid);
        unlock_vmid_allocator();
        return error_t::success;
    }

    [[nodiscard]] inline error_t ensure_vmid(virtual_machine_t& vm) noexcept {
        lock_vmid_allocator();
        const bool current = vm.vmid != 0U && vm.vmid < maximum_vmids &&
                             vm.vmid_generation == vmid_generation &&
                             (vmid_in_use & (1ULL << vm.vmid)) != 0U;
        unlock_vmid_allocator();
        if (current)
            return error_t::success;
        return allocate_vmid(vm.vmid, vm.vmid_generation);
    }
    [[nodiscard]] inline bool aligned(u64 value) noexcept {
        return (value & (page_size - 1U)) == 0U;
    }
    [[nodiscard]] inline bool contains_wx(u32 permissions) noexcept {
        return (permissions & static_cast<u32>(stage2_permission::write)) != 0U &&
               (permissions & static_cast<u32>(stage2_permission::execute)) != 0U;
    }
} // namespace sys::kernel::hypervisor
