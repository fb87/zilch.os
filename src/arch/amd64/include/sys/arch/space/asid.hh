#pragma once

#include <sys/arch/cpu.hh>
#include <sys/kernel/lock/order.hh>
#include <sys/types.hh>

namespace sys::arch::space::asid
{
    inline constexpr u32 capacity = 64U;
    inline constexpr u64 reserved = 1ULL;

    struct handle {
        u16 value{};
        u32 generation{};
    };

    inline volatile u32 allocator_lock{};
    inline u64 in_use{reserved};
    inline u32 generation{1U};
    inline u32 allocations{};
    inline u64 rollovers{};

    inline void lock() noexcept {
        while (__atomic_exchange_n(&allocator_lock, 1U, __ATOMIC_ACQUIRE) != 0U)
            arch::cpu::relax();
        kernel::lock_order::acquired(kernel::lock_order::rank::translation_identifier,
                                     &allocator_lock);
    }

    inline void unlock() noexcept {
        kernel::lock_order::released(kernel::lock_order::rank::translation_identifier,
                                     &allocator_lock);
        __atomic_store_n(&allocator_lock, 0U, __ATOMIC_RELEASE);
    }

    inline void invalidate_all() noexcept {
        /* Check if INVPCID is available: CPUID(7,0).EBX bit 10 */
        u32 eax = 7U, ecx = 0U;
        u32 ebx = 0U;
        __asm__ volatile("cpuid" : "+a"(eax), "+c"(ecx), "=b"(ebx));
        if ((ebx & (1U << 10U)) != 0U) {
            /* INVPCID available: type 2 (INVPCID_ALL_CONTEXT) invalidates all TLB */
            struct descriptor {
                u64 pcid;
                u64 linear_address;
            } desc{0U, 0U};
            __asm__ volatile("invpcid %1, %%rax" : : "a"(2U), "m"(desc));
        } else {
            /* Fallback: CR3 round-trip reload (portable, always available) */
            u64 cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
            __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));
        }
    }

    inline void rollover_locked() noexcept {
        invalidate_all();
        ++generation;
        if (generation == 0U)
            generation = 1U;
        in_use = reserved;
        allocations = 0U;
        ++rollovers;
    }

    [[nodiscard]] inline error_t allocate(handle& result) noexcept {
        lock();
        if (allocations >= capacity - 1U)
            rollover_locked();
        for (u32 candidate = 1U; candidate < capacity; ++candidate) {
            const u64 bit = 1ULL << candidate;
            if ((in_use & bit) != 0U)
                continue;
            in_use |= bit;
            ++allocations;
            result = {static_cast<u16>(candidate), generation};
            unlock();
            return error_t::success;
        }
        rollover_locked();
        in_use |= 1ULL << 1U;
        ++allocations;
        result = {1U, generation};
        unlock();
        return error_t::success;
    }

    inline void release(handle& value) noexcept {
        lock();
        if (value.generation == generation && value.value != 0U && value.value < capacity) {
            invalidate_all();
            in_use &= ~(1ULL << value.value);
        }
        unlock();
        value = {};
    }

    [[nodiscard]] inline error_t refresh(handle& value) noexcept {
        lock();
        const bool current = value.value != 0U && value.generation == generation &&
                             (in_use & (1ULL << value.value)) != 0U;
        unlock();
        if (current)
            return error_t::success;
        return allocate(value);
    }
} // namespace sys::arch::space::asid
