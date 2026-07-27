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
        __asm__ volatile("dsb ishst\n\ttlbi vmalle1is\n\tdsb ish\n\tisb" ::: "memory");
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
