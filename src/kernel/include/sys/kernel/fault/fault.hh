#pragma once

#include <sys/kernel/memory/object.hh>
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

    [[nodiscard]] inline constexpr error_t
    validate_mapping_reply(const record& pending, vaddr_t requested_page, vaddr_t fault_page,
                           memory::permission permissions) noexcept {
        if (pending.type == kind::none || requested_page != fault_page ||
            !memory::valid_permission(permissions))
            return error_t::invalid_argument;
        if (pending.type == kind::data_abort) {
            constexpr u64 data_abort_write = 1ULL << 6U;
            if (!memory::readable(permissions) ||
                ((pending.syndrome & data_abort_write) != 0U && !memory::writable(permissions)))
                return error_t::denied;
            return error_t::success;
        }
        if (pending.type == kind::instruction_abort)
            return memory::executable(permissions) ? error_t::success : error_t::denied;
        return error_t::invalid_argument;
    }
} // namespace sys::kernel::fault
