#pragma once

#include <sys/kernel/object/table.hh>
#include <sys/types.hh>

namespace sys::kernel::fault
{
    enum class kind : u8
    {
        none,
        instruction_abort,
        data_abort,
        alignment,
        invalid_context,
    };

    struct record
    {
        kind type{kind::none};
        thread_id_t thread{};
        u32 thread_generation{};
        u64 syndrome{};
        vaddr_t address{};
        vaddr_t instruction_pointer{};
    };

    enum class disposition : u8
    {
        pending,
        resume,
        terminate,
    };
}
