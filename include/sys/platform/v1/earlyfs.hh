#pragma once

#include <sys/types.hh>

namespace sys::platform::v1::earlyfs
{
    inline constexpr u8 magic[4] = {'Z', 'E', 'F', '1'};
    inline constexpr u32 format_version = 1U;
    inline constexpr u32 max_entries = 32U;
    inline constexpr u32 name_size = 48U;

    struct header_t {
        u8 magic[4];
        u32 version;
        u32 entry_count;
        u32 reserved;
    };
    static_assert(sizeof(header_t) == 16U);

    struct entry_t {
        char name[name_size];
        u64 offset;
        u64 size;
    };
    static_assert(sizeof(entry_t) == 64U);

    struct view {
        const u8* data{};
        usize_t size{};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return data != nullptr;
        }
    };

    [[nodiscard]] inline constexpr bool add_overflows(u64 left, u64 right) noexcept {
        return right > ~left;
    }

    /*
     * A standard NUL-terminated-string comparison against a fixed-size
     * field: the two sides can only ever diverge (and return) at or before
     * whichever string's NUL comes first, so this never reads past query's
     * own NUL terminator, and never reads past name_size bytes of
     * entry_name. If entry_name fills all name_size bytes with no NUL (only
     * possible from a malformed/adversarial image -- the packer always
     * leaves room for one), that entry can't match any query and is
     * correctly treated as a non-match rather than read further.
     */
    [[nodiscard]] inline bool name_equal(const char* entry_name, const char* query) noexcept {
        for (u32 index = 0U; index < name_size; ++index) {
            const char lhs = entry_name[index];
            const char rhs = query[index];
            if (lhs != rhs)
                return false;
            if (lhs == '\0')
                return true;
        }
        return false;
    }

    /*
     * Bounded, allocation-free lookup into a ZEFS1 image already mapped
     * into this address space. Every check is a hard bounds check against
     * image_size before any offset derived from the image's own contents is
     * trusted -- the image is untrusted input (it crosses a build/runtime
     * boundary, and in later phases may cross a process boundary).
     */
    [[nodiscard]] inline view find(const u8* image, usize_t image_size,
                                   const char* name) noexcept {
        if (image == nullptr || name == nullptr || image_size < sizeof(header_t))
            return {};
        const auto* header = reinterpret_cast<const header_t*>(image);
        if (header->magic[0] != magic[0] || header->magic[1] != magic[1] ||
            header->magic[2] != magic[2] || header->magic[3] != magic[3] ||
            header->version != format_version || header->entry_count > max_entries)
            return {};
        const u64 directory_bytes = static_cast<u64>(header->entry_count) * sizeof(entry_t);
        if (add_overflows(sizeof(header_t), directory_bytes) ||
            sizeof(header_t) + directory_bytes > image_size)
            return {};
        const auto* entries = reinterpret_cast<const entry_t*>(image + sizeof(header_t));
        for (u32 index = 0U; index < header->entry_count; ++index) {
            const entry_t& entry = entries[index];
            if (!name_equal(entry.name, name))
                continue;
            if (add_overflows(entry.offset, entry.size) || entry.offset + entry.size > image_size)
                return {};
            return {image + entry.offset, static_cast<usize_t>(entry.size)};
        }
        return {};
    }
} // namespace sys::platform::v1::earlyfs
