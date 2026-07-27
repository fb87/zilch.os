#pragma once

#include <sys/types.hh>

namespace sys::kernel::boot::fdt
{
    inline constexpr u32 magic = 0xd00dfeedU;
    inline constexpr u32 begin_node = 1U;
    inline constexpr u32 end_node = 2U;
    inline constexpr u32 property = 3U;
    inline constexpr u32 nop = 4U;
    inline constexpr u32 end = 9U;

    struct range {
        paddr_t base{};
        psize_t size{};
    };
    struct inventory {
        static constexpr u32 maximum_memory_ranges = 8U;
        static constexpr u32 maximum_reserved_ranges = 16U;
        range memory[maximum_memory_ranges]{};
        range reserved[maximum_reserved_ranges]{};
        u32 memory_count{};
        u32 reserved_count{};
        paddr_t blob_base{};
        psize_t blob_size{};
    };

    [[nodiscard]] inline u32 be32(const void* pointer) noexcept {
        const auto* p = static_cast<const u8*>(pointer);
        return (static_cast<u32>(p[0]) << 24U) | (static_cast<u32>(p[1]) << 16U) |
               (static_cast<u32>(p[2]) << 8U) | static_cast<u32>(p[3]);
    }

    [[nodiscard]] inline u64 read_cells(const u8* data, u32 cells) noexcept {
        u64 value{};
        for (u32 i = 0U; i < cells; ++i)
            value = (value << 32U) | be32(data + i * 4U);
        return value;
    }

    [[nodiscard]] inline bool equal(const char* left, const char* right) noexcept {
        while (*left != '\0' && *right != '\0' && *left == *right) {
            ++left;
            ++right;
        }
        return *left == *right;
    }

    [[nodiscard]] inline bool starts_with(const char* text, const char* prefix) noexcept {
        while (*prefix != '\0') {
            if (*text++ != *prefix++)
                return false;
        }
        return true;
    }

    inline bool append(range* ranges, u32 capacity, u32& count, paddr_t base,
                       psize_t size) noexcept {
        if (size == 0U || count >= capacity || base + size < base)
            return false;
        ranges[count++] = {base, size};
        return true;
    }

    [[nodiscard]] inline error_t parse(uintptr_t address, inventory& output) noexcept {
        output.memory_count = 0U;
        output.reserved_count = 0U;
        output.blob_base = 0U;
        output.blob_size = 0U;
        for (auto& item : output.memory)
            item = {};
        for (auto& item : output.reserved)
            item = {};
        if (address == 0U)
            return error_t::not_found;
        const auto* blob = reinterpret_cast<const u8*>(address);
        if (be32(blob) != magic)
            return error_t::invalid_argument;
        const u32 total = be32(blob + 4U);
        const u32 structure_offset = be32(blob + 8U);
        const u32 strings_offset = be32(blob + 12U);
        const u32 reserve_offset = be32(blob + 16U);
        const u32 strings_size = be32(blob + 32U);
        const u32 structure_size = be32(blob + 36U);
        if (total < 40U || structure_offset > total || strings_offset > total ||
            reserve_offset > total || structure_size > total - structure_offset ||
            strings_size > total - strings_offset)
            return error_t::invalid_argument;
        output.blob_base = static_cast<paddr_t>(address);
        output.blob_size = total;

        const u8* reserve = blob + reserve_offset;
        const u8* blob_end = blob + total;
        while (reserve + 16U <= blob_end) {
            const u64 base = (static_cast<u64>(be32(reserve)) << 32U) | be32(reserve + 4U);
            const u64 size = (static_cast<u64>(be32(reserve + 8U)) << 32U) | be32(reserve + 12U);
            reserve += 16U;
            if (base == 0U && size == 0U)
                break;
            if (!append(output.reserved, inventory::maximum_reserved_ranges, output.reserved_count,
                        base, size))
                return error_t::no_memory;
        }
        if (!append(output.reserved, inventory::maximum_reserved_ranges, output.reserved_count,
                    output.blob_base, output.blob_size))
            return error_t::no_memory;

        const u8* cursor = blob + structure_offset;
        const u8* structure_end = cursor + structure_size;
        const char* strings = reinterpret_cast<const char*>(blob + strings_offset);
        u32 depth{};
        u32 root_address_cells = 2U;
        u32 root_size_cells = 2U;
        bool memory_node[16]{};
        bool reserved_container[16]{};
        bool reserved_child[16]{};

        while (cursor + 4U <= structure_end) {
            const u32 token = be32(cursor);
            cursor += 4U;
            if (token == begin_node) {
                if (depth >= 16U)
                    return error_t::invalid_argument;
                const char* name = reinterpret_cast<const char*>(cursor);
                const u8* scan = cursor;
                while (scan < structure_end && *scan != 0U)
                    ++scan;
                if (scan == structure_end)
                    return error_t::invalid_argument;
                cursor = reinterpret_cast<const u8*>((reinterpret_cast<uintptr_t>(scan + 1U) + 3U) &
                                                     ~3ULL);
                memory_node[depth] = starts_with(name, "memory@");
                reserved_container[depth] = depth == 1U && equal(name, "reserved-memory");
                reserved_child[depth] = depth > 1U && reserved_container[depth - 1U];
                ++depth;
            } else if (token == end_node) {
                if (depth == 0U)
                    return error_t::invalid_argument;
                --depth;
            } else if (token == property) {
                if (cursor + 8U > structure_end)
                    return error_t::invalid_argument;
                const u32 length = be32(cursor);
                const u32 name_offset = be32(cursor + 4U);
                cursor += 8U;
                if (length > static_cast<u32>(structure_end - cursor) ||
                    name_offset >= strings_size)
                    return error_t::invalid_argument;
                const char* name = strings + name_offset;
                const u8* value = cursor;
                cursor = reinterpret_cast<const u8*>(
                    (reinterpret_cast<uintptr_t>(cursor + length) + 3U) & ~3ULL);
                if (depth == 1U && equal(name, "#address-cells") && length == 4U)
                    root_address_cells = be32(value);
                else if (depth == 1U && equal(name, "#size-cells") && length == 4U)
                    root_size_cells = be32(value);
                else if (depth != 0U && equal(name, "device_type") && length >= 7U &&
                         equal(reinterpret_cast<const char*>(value), "memory"))
                    memory_node[depth - 1U] = true;
                else if (depth != 0U && equal(name, "reg")) {
                    const bool is_memory = memory_node[depth - 1U];
                    const bool is_reserved = reserved_child[depth - 1U];
                    if (!is_memory && !is_reserved)
                        continue;
                    if (root_address_cells == 0U || root_address_cells > 2U ||
                        root_size_cells == 0U || root_size_cells > 2U)
                        return error_t::unsupported;
                    const u32 tuple_bytes = (root_address_cells + root_size_cells) * 4U;
                    if (tuple_bytes == 0U || length % tuple_bytes != 0U)
                        return error_t::invalid_argument;
                    for (u32 offset = 0U; offset < length; offset += tuple_bytes) {
                        const paddr_t base = read_cells(value + offset, root_address_cells);
                        const psize_t size =
                            read_cells(value + offset + root_address_cells * 4U, root_size_cells);
                        if (is_memory) {
                            if (!append(output.memory, inventory::maximum_memory_ranges,
                                        output.memory_count, base, size))
                                return error_t::no_memory;
                        } else if (!append(output.reserved, inventory::maximum_reserved_ranges,
                                           output.reserved_count, base, size))
                            return error_t::no_memory;
                    }
                }
            } else if (token == nop) {
                continue;
            } else if (token == end) {
                return output.memory_count != 0U ? error_t::success : error_t::not_found;
            } else
                return error_t::invalid_argument;
        }
        return error_t::invalid_argument;
    }
} // namespace sys::kernel::boot::fdt
