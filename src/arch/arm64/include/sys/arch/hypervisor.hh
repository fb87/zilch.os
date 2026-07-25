#pragma once

#include <sys/arch/cpu.hh>
#include <sys/arch/exception.hh>
#include <sys/types.hh>

namespace sys::arch::hypervisor
{
    inline constexpr bool available = true;
    inline constexpr bool active = true;
    inline constexpr u64 abi_signature = 0x5a494c4300000000ULL;
    inline constexpr u64 abi_version = 0x0000000000010000ULL;
    inline constexpr u64 test_cookie = 0x123456789abcdef0ULL;
    inline constexpr u32 maximum_cpu_count = 4U;

    enum class call_id : u64
    {
        version = abi_signature | 0x01ULL,
        cpu_id = abi_signature | 0x02ULL,
        test = abi_signature | 0x03ULL,
    };

    inline volatile u32 verified_cpu_mask = 0U;
    inline volatile u64 call_count[maximum_cpu_count]{};

    [[nodiscard]] inline u64 call(
        call_id function,
        u64 argument0 = 0U,
        u64 argument1 = 0U,
        u64 argument2 = 0U) noexcept
    {
        register u64 x0 __asm__("x0") = static_cast<u64>(function);
        register u64 x1 __asm__("x1") = argument0;
        register u64 x2 __asm__("x2") = argument1;
        register u64 x3 __asm__("x3") = argument2;

        __asm__ volatile(
            "hvc #0"
            : "+r"(x0)
            : "r"(x1), "r"(x2), "r"(x3)
            : "x4", "x5", "x6", "x7", "memory");

        return x0;
    }

    [[nodiscard]] inline error_t initialize() noexcept
    {
        return error_t::success;
    }

    [[nodiscard]] inline bool initialize_cpu() noexcept
    {
        const cpu_id_t id = cpu::current_id();
        if (id >= maximum_cpu_count) {
            return false;
        }

        if (call(call_id::version) != abi_version) {
            return false;
        }
        if (call(call_id::cpu_id) != static_cast<u64>(id)) {
            return false;
        }
        if (call(call_id::test, test_cookie) != test_cookie) {
            return false;
        }

        __atomic_fetch_or(&verified_cpu_mask, 1U << id, __ATOMIC_RELEASE);
        return true;
    }

    [[nodiscard]] inline u32 verified_count() noexcept
    {
        return static_cast<u32>(__builtin_popcount(
            __atomic_load_n(&verified_cpu_mask, __ATOMIC_ACQUIRE)));
    }

    [[nodiscard]] inline bool dispatch(
        exception::frame_t& frame,
        u64 syndrome) noexcept
    {
        constexpr u64 exception_class_mask = 0x3fULL;
        constexpr u64 hvc64_exception_class = 0x16ULL;
        const u64 exception_class = (syndrome >> 26U) & exception_class_mask;
        if (exception_class != hvc64_exception_class) {
            return false;
        }

        const cpu_id_t id = cpu::current_id();
        if (id < maximum_cpu_count) {
            __atomic_fetch_add(&call_count[id], 1U, __ATOMIC_RELAXED);
        }

        switch (static_cast<call_id>(frame.x[0])) {
        case call_id::version:
            frame.x[0] = abi_version;
            break;
        case call_id::cpu_id:
            frame.x[0] = static_cast<u64>(id);
            break;
        case call_id::test:
            frame.x[0] = frame.x[1];
            break;
        default:
            frame.x[0] = static_cast<u64>(static_cast<s64>(-1));
            break;
        }
        return true;
    }
} // namespace sys::arch::hypervisor
