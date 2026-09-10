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

    /*
     * One slot per CPU holding the tag that CPU currently has installed in
     * TTBR0, maintained by address_space::activate() and cleared by
     * activate_kernel(). It is what makes "is this tag live somewhere?"
     * answerable, which is the question the allocator has to be able to
     * ask: recycling a tag that a CPU is still translating through leaves
     * two live address spaces sharing one TLB tag, and the first one's
     * cached entries then answer for the second.
     *
     * address_space::active_cpu_mask records the same residency from the
     * space's side, but it is only ever set, never cleared, so it cannot
     * answer this. This is the allocator's own view and is authoritative.
     */
    inline constexpr u32 maximum_cpu_count = 4U;
    inline volatile u16 installed[maximum_cpu_count]{};

    inline volatile u32 allocator_lock{};
    inline u64 in_use{reserved};
    inline u32 generation{1U};
    /*
     * LIVE allocations, not lifetime ones. This used to only ever
     * increment -- release() cleared the in_use bit but left the counter
     * alone -- so the rollover trigger below fired every 63 allocations
     * ever made, regardless of how few were actually outstanding. With at
     * most 16 live address spaces (user_thread_count) against 63 usable
     * tags, rollover should be unreachable in this kernel; process churn
     * during boot reached it in well under a second, and every rollover
     * reassigns tags underneath CPUs that are still using them.
     */
    inline u32 allocations{};
    inline u64 rollovers{};

    [[nodiscard]] inline u64 installed_mask() noexcept {
        u64 mask = 0U;
        for (u32 cpu = 0U; cpu < maximum_cpu_count; ++cpu) {
            const u16 tag = __atomic_load_n(&installed[cpu], __ATOMIC_ACQUIRE);
            if (tag != 0U && tag < capacity)
                mask |= 1ULL << tag;
        }
        return mask;
    }

    /*
     * Called by activate() with the tag it just put into TTBR0, and by
     * activate_kernel() with 0. Writes only this CPU's own slot, so it
     * needs no lock: the allocator reads every slot, but a tag can only
     * become installed if it was already allocated, so a slot changing
     * under a reader can never make a free tag look busy or vice versa.
     */
    inline void note_installed(u32 cpu, u16 tag) noexcept {
        if (cpu < maximum_cpu_count)
            __atomic_store_n(&installed[cpu], tag, __ATOMIC_RELEASE);
    }

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

    /*
     * Tags still installed on a CPU survive a rollover, reserved rather
     * than freed. Clearing them wholesale is what made rollover corrupting
     * rather than merely expensive: every live space reallocates on its
     * next refresh() while the CPUs running them keep the old tags in
     * TTBR0, and the freed values are immediately handed to new spaces.
     *
     * The in-flight tags are re-reserved in the NEW generation, so they
     * stay unavailable until the CPU holding one installs something else.
     */
    inline void rollover_locked() noexcept {
        invalidate_all();
        ++generation;
        if (generation == 0U)
            generation = 1U;
        const u64 live = installed_mask();
        in_use = reserved | live;
        allocations = static_cast<u32>(__builtin_popcountll(live));
        ++rollovers;
    }

    [[nodiscard]] inline error_t allocate(handle& result) noexcept {
        lock();
        if (allocations >= capacity - 1U)
            rollover_locked();
        // Never hand out a tag a CPU is still translating through, even if
        // its in_use bit is clear -- a released space whose CPU has not yet
        // switched away still owns the TLB entries under that tag.
        const u64 unavailable = in_use | installed_mask();
        for (u32 candidate = 1U; candidate < capacity; ++candidate) {
            const u64 bit = 1ULL << candidate;
            if ((unavailable & bit) != 0U)
                continue;
            in_use |= bit;
            ++allocations;
            result = {static_cast<u16>(candidate), generation};
            unlock();
            return error_t::success;
        }
        rollover_locked();
        /*
         * Genuinely exhausted: every tag is either reserved or pinned by a
         * CPU. Fail rather than duplicate one. The caller (initialize()/
         * clone()) already propagates the error and rolls the space back,
         * whereas the previous behaviour handed out tag 1 unconditionally
         * -- aliasing it with whoever already held it.
         */
        const u64 still_unavailable = in_use | installed_mask();
        for (u32 candidate = 1U; candidate < capacity; ++candidate) {
            const u64 bit = 1ULL << candidate;
            if ((still_unavailable & bit) != 0U)
                continue;
            in_use |= bit;
            ++allocations;
            result = {static_cast<u16>(candidate), generation};
            unlock();
            return error_t::success;
        }
        unlock();
        return error_t::no_memory;
    }

    inline void release(handle& value) noexcept {
        lock();
        if (value.generation == generation && value.value != 0U && value.value < capacity &&
            (in_use & (1ULL << value.value)) != 0U) {
            invalidate_all();
            in_use &= ~(1ULL << value.value);
            // Keep `allocations` a live count; see its declaration.
            if (allocations != 0U)
                --allocations;
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
