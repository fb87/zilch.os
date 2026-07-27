#pragma once

#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

#include <abi/sys/v1/fault.hh>

namespace sys::kernel::fault
{
    enum class kind : u8 {
        none,
        instruction_abort,
        data_abort,
        alignment,
        invalid_context,
    };

    struct record {
        kind type{kind::none};
        thread_id_t thread{};
        u32 thread_generation{};
        u64 syndrome{};
        vaddr_t address{};
        vaddr_t instruction_pointer{};
    };

    enum class disposition : u8 {
        pending,
        resume,
        terminate,
    };

    static_assert(static_cast<u8>(kind::instruction_abort) ==
                  static_cast<u8>(abi::v1::fault_kind::instruction_abort));
    static_assert(static_cast<u8>(kind::data_abort) ==
                  static_cast<u8>(abi::v1::fault_kind::data_abort));
    static_assert(static_cast<u8>(disposition::resume) ==
                  static_cast<u8>(abi::v1::fault_disposition::resume));
    static_assert(static_cast<u8>(disposition::terminate) ==
                  static_cast<u8>(abi::v1::fault_disposition::terminate));
} // namespace sys::kernel::fault
