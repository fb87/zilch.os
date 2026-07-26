#pragma once

#include <sys/arch/arch.hh>
#include <sys/kernel/object.hh>

namespace sys::kernel::memory
{
    struct address_space_t {
        object::header_t header;
        space_id_t id;
        paddr_t root_table;
        u16 address_space_identifier;
    };

    [[nodiscard]] inline error_t map(address_space_t& address_space, vaddr_t virtual_address,
                                     paddr_t physical_address,
                                     arch::page_attributes_t attributes) noexcept {
        return arch::memory::map_page(address_space.root_table, virtual_address, physical_address,
                                      attributes);
    }
} // namespace sys::kernel::memory
