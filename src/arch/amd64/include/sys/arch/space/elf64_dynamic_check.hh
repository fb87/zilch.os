#pragma once

#include <sys/arch/space/address_space.hh>

namespace sys::arch::space
{
    // No ELF loader (dynamic or otherwise) on this arch yet; nothing to validate.
    [[nodiscard]] inline bool validate_elf64_dynamic_loader() noexcept {
        return true;
    }
} // namespace sys::arch::space
