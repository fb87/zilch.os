#pragma once

#include <sys/arch/cpu.hh>
#include <sys/types.hh>

namespace sys::arch::stack
{
    inline constexpr usize_t cpu_count = 4U;
    inline constexpr usize_t stack_size = 0x8000U;
    inline constexpr usize_t canary_words = 8U;
    inline constexpr u64 canary = 0x5a494c434b53544bULL;

    extern "C" char __cpu_stacks_start[];
    extern "C" char __hypervisor_stacks_start[];

    inline uintptr_t el1_low_water[cpu_count]{};
    inline uintptr_t el2_low_water[cpu_count]{};
    inline u32 initialized[cpu_count]{};

    [[nodiscard]] inline uintptr_t bottom(u64 level, u32 cpu) noexcept {
        const uintptr_t start = reinterpret_cast<uintptr_t>(level == 2U ? __hypervisor_stacks_start
                                                                        : __cpu_stacks_start);
        return start + static_cast<uintptr_t>(cpu) * stack_size;
    }

    [[nodiscard]] inline uintptr_t top(u64 level, u32 cpu) noexcept {
        return bottom(level, cpu) + stack_size;
    }

    inline void seed(uintptr_t base) noexcept {
        auto* words = reinterpret_cast<volatile u64*>(base);
        for (usize_t index = 0U; index < canary_words; ++index)
            words[index] = canary;
    }

    [[nodiscard]] inline bool canary_valid(uintptr_t base) noexcept {
        const auto* words = reinterpret_cast<const volatile u64*>(base);
        for (usize_t index = 0U; index < canary_words; ++index) {
            if (words[index] != canary)
                return false;
        }
        return true;
    }

    inline void initialize_current_cpu() noexcept {
        const u32 cpu = cpu::current_id();
        if (cpu >= cpu_count)
            return;
        const uintptr_t el1_bottom = bottom(1U, cpu);
        const uintptr_t el2_bottom = bottom(2U, cpu);
        seed(el1_bottom);
        seed(el2_bottom);
        __atomic_store_n(&el1_low_water[cpu], top(1U, cpu), __ATOMIC_RELEASE);
        __atomic_store_n(&el2_low_water[cpu], top(2U, cpu), __ATOMIC_RELEASE);
        __atomic_store_n(&initialized[cpu], 1U, __ATOMIC_RELEASE);
    }

    [[nodiscard]] inline bool observe(u64 level) noexcept {
        const u32 cpu = cpu::current_id();
        if (cpu >= cpu_count || __atomic_load_n(&initialized[cpu], __ATOMIC_ACQUIRE) == 0U)
            return false;
        uintptr_t current_sp{};
        __asm__ volatile("mov %0, sp" : "=r"(current_sp));
        const uintptr_t base = bottom(level, cpu);
        const uintptr_t limit = top(level, cpu);
        if (current_sp < base + canary_words * sizeof(u64) || current_sp > limit ||
            !canary_valid(base))
            return false;
        uintptr_t* low_water = level == 2U ? &el2_low_water[cpu] : &el1_low_water[cpu];
        uintptr_t observed = __atomic_load_n(low_water, __ATOMIC_ACQUIRE);
        while (current_sp < observed &&
               !__atomic_compare_exchange_n(low_water, &observed, current_sp, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        }
        return true;
    }

    [[nodiscard]] inline usize_t maximum_observed_usage(u64 level, u32 cpu) noexcept {
        if (cpu >= cpu_count)
            return stack_size;
        const uintptr_t observed = __atomic_load_n(
            level == 2U ? &el2_low_water[cpu] : &el1_low_water[cpu], __ATOMIC_ACQUIRE);
        return observed == 0U ? stack_size : top(level, cpu) - observed;
    }
} // namespace sys::arch::stack
