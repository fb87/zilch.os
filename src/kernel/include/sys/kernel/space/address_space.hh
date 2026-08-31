#pragma once

#include <sys/arch/space/address_space.hh>
#include <sys/kernel/object.hh>
#include <sys/types.hh>

namespace sys::kernel::space
{
    struct address_space {
        object::header_t object{};
        space_id_t id{};
        arch::space::address_space native{};

        [[nodiscard]] inline error_t initialize(space_id_t space_id, word_t image_role,
                                                arch::space::page_allocate_fn allocate_page,
                                                arch::space::page_release_fn release_page) noexcept {
            id = space_id;
            return arch::space::initialize(native, image_role, allocate_page, release_page);
        }

        inline void activate() noexcept {
            arch::space::activate(native);
        }

        inline void release(arch::space::page_release_fn release_page) noexcept {
            arch::space::release(native, release_page);
        }

        [[nodiscard]] inline error_t map_page(vaddr_t address, void* page, bool writable,
                                              bool executable, bool device,
                                              bool inner_shareable) noexcept {
            return arch::space::map_page(native, address, page, writable, executable, device,
                                         inner_shareable);
        }

        [[nodiscard]] inline error_t unmap_page(vaddr_t address) noexcept {
            return arch::space::unmap_page(native, address);
        }
    };
} // namespace sys::kernel::space
