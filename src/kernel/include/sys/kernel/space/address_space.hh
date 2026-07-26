#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::space
{
    struct address_space
    {
        object::header_t object{};
        space_id_t id{};
        arch::space::address_space native{};

        inline void initialize(u16 asid) noexcept
        {
            id = static_cast<space_id_t>(asid);
            arch::space::initialize(native, asid);
        }

        inline void activate() noexcept
        {
            arch::space::activate(native);
        }

        [[nodiscard]] inline error_t map_page(vaddr_t address, void* page,
                                              bool writable,
                                              bool executable) noexcept
        {
            return arch::space::map_page(native, address, page, writable,
                                         executable);
        }

        [[nodiscard]] inline error_t unmap_page(vaddr_t address) noexcept
        {
            return arch::space::unmap_page(native, address);
        }
    };
} // namespace sys::kernel::space
