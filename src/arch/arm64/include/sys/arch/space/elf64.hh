#pragma once

#include <sys/arch/memory.hh>
#include <sys/types.hh>

namespace sys::arch::space::elf64
{
    inline constexpr u8 elf_class_64 = 2U;
    inline constexpr u8 elf_data_little = 1U;
    inline constexpr u16 elf_machine_aarch64 = 183U;
    inline constexpr u32 program_type_load = 1U;
    inline constexpr u32 program_flag_execute = 1U;
    inline constexpr u32 program_flag_write = 2U;
    inline constexpr usize_t bootstrap_pages = 16U;
    inline constexpr usize_t bootstrap_size = bootstrap_pages * memory::page_size;

    struct header {
        u8 ident[16];
        u16 type;
        u16 machine;
        u32 version;
        u64 entry;
        u64 program_offset;
        u64 section_offset;
        u32 flags;
        u16 header_size;
        u16 program_entry_size;
        u16 program_count;
        u16 section_entry_size;
        u16 section_count;
        u16 section_names;
    };

    struct program_header {
        u32 type;
        u32 flags;
        u64 offset;
        u64 virtual_address;
        u64 physical_address;
        u64 file_size;
        u64 memory_size;
        u64 alignment;
    };

    struct page_permissions {
        bool present{};
        bool writable{};
        bool executable{};
    };

    struct result {
        error_t status{error_t::invalid_argument};
        vaddr_t entry{};
        usize_t pages{};
    };

    [[nodiscard]] inline bool add_overflows(u64 left, u64 right) noexcept {
        return right > (~static_cast<u64>(0U) - left);
    }

    [[nodiscard]] inline bool valid_header(const header& value) noexcept {
        return value.ident[0] == 0x7fU && value.ident[1] == 'E' && value.ident[2] == 'L' &&
               value.ident[3] == 'F' && value.ident[4] == elf_class_64 &&
               value.ident[5] == elf_data_little && value.ident[6] == 1U &&
               value.machine == elf_machine_aarch64 && value.version == 1U &&
               value.header_size == sizeof(header) &&
               value.program_entry_size == sizeof(program_header) && value.program_count != 0U;
    }

    [[nodiscard]] inline result load(const u8* image, usize_t image_size, vaddr_t base,
                                     u8 (&storage)[bootstrap_size],
                                     page_permissions (&permissions)[bootstrap_pages]) noexcept {
        for (usize_t index = 0U; index < bootstrap_size; ++index)
            storage[index] = 0U;
        for (usize_t index = 0U; index < bootstrap_pages; ++index)
            permissions[index] = {};

        if (image == nullptr || image_size < sizeof(header))
            return {};
        const auto* elf = reinterpret_cast<const header*>(image);
        if (!valid_header(*elf))
            return {};
        if (add_overflows(elf->program_offset,
                          static_cast<u64>(elf->program_count) * sizeof(program_header)))
            return {};
        const u64 program_end =
            elf->program_offset + static_cast<u64>(elf->program_count) * sizeof(program_header);
        if (program_end > image_size)
            return {};

        bool entry_executable = false;
        usize_t highest_page = 0U;
        for (u16 index = 0U; index < elf->program_count; ++index) {
            const u64 location =
                elf->program_offset + static_cast<u64>(index) * sizeof(program_header);
            const auto* segment =
                reinterpret_cast<const program_header*>(image + static_cast<usize_t>(location));
            if (segment->type != program_type_load)
                continue;
            if (segment->file_size > segment->memory_size ||
                add_overflows(segment->offset, segment->file_size) ||
                segment->offset + segment->file_size > image_size ||
                add_overflows(segment->virtual_address, segment->memory_size))
                return {};
            if (segment->alignment != 0U && segment->alignment != 1U &&
                segment->alignment != memory::page_size)
                return {};
            if (segment->alignment == memory::page_size &&
                ((segment->virtual_address - segment->offset) & (memory::page_size - 1U)) != 0U)
                return {};
            if ((segment->flags & program_flag_write) != 0U &&
                (segment->flags & program_flag_execute) != 0U)
                return {};
            if (segment->virtual_address < base)
                return {};
            const u64 relative = segment->virtual_address - base;
            if (relative > bootstrap_size || segment->memory_size > bootstrap_size - relative)
                return {};

            const u64 segment_end = relative + segment->memory_size;
            const usize_t first_page = static_cast<usize_t>(relative / memory::page_size);
            const usize_t end_page =
                static_cast<usize_t>((segment_end + memory::page_size - 1U) / memory::page_size);
            if (end_page > bootstrap_pages)
                return {};
            for (usize_t page = first_page; page < end_page; ++page) {
                if (permissions[page].present)
                    return {};
                permissions[page].present = true;
                permissions[page].writable = (segment->flags & program_flag_write) != 0U;
                permissions[page].executable = (segment->flags & program_flag_execute) != 0U;
            }
            for (u64 byte = 0U; byte < segment->file_size; ++byte) {
                storage[static_cast<usize_t>(relative + byte)] =
                    image[static_cast<usize_t>(segment->offset + byte)];
            }
            if (end_page > highest_page)
                highest_page = end_page;
            if (elf->entry >= segment->virtual_address &&
                elf->entry < segment->virtual_address + segment->memory_size &&
                (segment->flags & program_flag_execute) != 0U)
                entry_executable = true;
        }
        if (!entry_executable)
            return {};
        return {error_t::success, elf->entry, highest_page};
    }
} // namespace sys::arch::space::elf64
