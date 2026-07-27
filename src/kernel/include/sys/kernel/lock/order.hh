#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::kernel::lock_order
{
    enum class rank : u16 {
        endpoint = 10U,
        ipc_lifecycle = 20U,
        capability_authority = 30U,
        capability_registry = 40U,
        memory_mapping = 40U,
        cspace = 50U,
        capability_derivation = 60U,
        memory_allocator = 70U,
        translation_identifier = 75U,
        object_table = 80U,
    };

    inline constexpr u32 maximum_cpus = 64U;
    inline constexpr u32 maximum_depth = 16U;

#if CONFIG_SELFTEST
    struct held_lock {
        rank order{};
        const volatile void* identity{};
    };

    inline held_lock held[maximum_cpus][maximum_depth]{};
    inline u32 depth[maximum_cpus]{};
    inline volatile u64 violations{};

    [[nodiscard]] inline bool may_acquire(rank order, const volatile void* identity) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= maximum_cpus || identity == nullptr)
            return false;
        const u32 current_depth = depth[cpu];
        if (current_depth >= maximum_depth)
            return false;
        if (current_depth == 0U)
            return true;
        const held_lock& current = held[cpu][current_depth - 1U];
        const auto requested_rank = static_cast<u16>(order);
        const auto current_rank = static_cast<u16>(current.order);
        if (requested_rank < current_rank)
            return false;
        return requested_rank != current_rank || reinterpret_cast<uintptr_t>(identity) >
                                                     reinterpret_cast<uintptr_t>(current.identity);
    }

    inline void acquired(rank order, const volatile void* identity) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (!may_acquire(order, identity)) {
            __atomic_fetch_add(&violations, 1U, __ATOMIC_RELAXED);
            return;
        }
        held[cpu][depth[cpu]++] = {order, identity};
    }

    inline void released(rank order, const volatile void* identity) noexcept {
        const cpu_id_t cpu = arch::cpu::current_id();
        if (cpu >= maximum_cpus || depth[cpu] == 0U) {
            __atomic_fetch_add(&violations, 1U, __ATOMIC_RELAXED);
            return;
        }
        const held_lock& current = held[cpu][depth[cpu] - 1U];
        if (current.order != order || current.identity != identity) {
            __atomic_fetch_add(&violations, 1U, __ATOMIC_RELAXED);
            return;
        }
        held[cpu][--depth[cpu]] = {};
    }

    [[nodiscard]] inline u64 violation_count() noexcept {
        return __atomic_load_n(&violations, __ATOMIC_ACQUIRE);
    }
#else
    [[nodiscard]] inline bool may_acquire(rank, const volatile void*) noexcept {
        return true;
    }
    inline void acquired(rank, const volatile void*) noexcept {}
    inline void released(rank, const volatile void*) noexcept {}
    [[nodiscard]] inline u64 violation_count() noexcept {
        return 0U;
    }
#endif
} // namespace sys::kernel::lock_order
