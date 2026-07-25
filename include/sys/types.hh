#pragma once

namespace sys
{
    using s8 = __INT8_TYPE__;
    using u8 = __UINT8_TYPE__;
    using s16 = __INT16_TYPE__;
    using u16 = __UINT16_TYPE__;
    using s32 = __INT32_TYPE__;
    using u32 = __UINT32_TYPE__;
    using s64 = __INT64_TYPE__;
    using u64 = __UINT64_TYPE__;

    using usize_t = __SIZE_TYPE__;
    using isize_t = __PTRDIFF_TYPE__;
    using uintptr_t = __UINTPTR_TYPE__;
    using intptr_t = __INTPTR_TYPE__;

    using word_t = uintptr_t;
    using reg_t = uintptr_t;
    using paddr_t = u64;
    using psize_t = u64;
    using vaddr_t = uintptr_t;
    using vsize_t = usize_t;

    using cpu_id_t = u32;
    using irq_id_t = u32;
    using thread_id_t = u64;
    using space_id_t = u64;
    using object_id_t = u64;
    using capability_id_t = u64;
    using endpoint_id_t = u64;
    using vm_id_t = u64;
    using vcpu_id_t = u64;
    using time_ns_t = u64;

    enum class error_t : s32 {
        success = 0,
        invalid_argument = -1,
        unsupported = -2,
        no_memory = -3,
        denied = -4,
        busy = -5,
        not_found = -6,
        timed_out = -7,
    };

    inline constexpr usize_t bits_per_byte = 8U;
    inline constexpr usize_t word_bits = sizeof(word_t) * bits_per_byte;

    static_assert(sizeof(s8) == 1U);
    static_assert(sizeof(u8) == 1U);
    static_assert(sizeof(s16) == 2U);
    static_assert(sizeof(u16) == 2U);
    static_assert(sizeof(s32) == 4U);
    static_assert(sizeof(u32) == 4U);
    static_assert(sizeof(s64) == 8U);
    static_assert(sizeof(u64) == 8U);
    static_assert(sizeof(paddr_t) == 8U);
    static_assert(sizeof(word_t) == sizeof(void*));
    static_assert(word_bits == 64U);
} // namespace sys
