#pragma once

#include <string.h>

#include <sys/ext2/format.hh>
#include <sys/types.hh>

/*
 * Read-only ext2 traversal: mount, resolve a path, read file bytes, list a
 * directory. No dynamic allocation and no recursion -- every buffer is a
 * caller- or stack-local fixed array, and path resolution is an iterative
 * walk bounded by the path string's own length.
 *
 * I/O is abstracted to one callback reading a fixed 1024-byte unit, chosen
 * because the on-disk superblock lives at a fixed byte offset (1024)
 * regardless of the filesystem's own block size, so mounting has to read in
 * 1024-byte units before block size is even known. Every ext2 block is then
 * read as 1, 2, or 4 such units, keeping exactly one I/O primitive for the
 * whole reader instead of a bootstrap-only one plus a block-sized one.
 */
namespace sys::ext2
{
    inline constexpr u32 max_block_size = 4096U;
    inline constexpr u32 kib_size = 1024U;
    inline constexpr u32 max_pointers_per_block = max_block_size / sizeof(u32);

    using read_kib_fn = bool (*)(void* context, u32 index, u8 (&destination)[kib_size]) noexcept;

    struct filesystem final {
        read_kib_fn read_kib{};
        void* context{};
        superblock sb{};
        u32 block_size{};
        u32 kib_per_block{};
        u32 pointers_per_block{};
        u32 group_count{};
    };

    [[nodiscard]] inline bool read_block_bytes(const filesystem& fs, u32 block_number,
                                               u8* destination) noexcept {
        // Block 0 is the boot block / superblock region, never a valid
        // pointer target; treating it as one would silently re-read the
        // filesystem's own metadata as if it were file content.
        if (block_number == 0U)
            return false;
        for (u32 unit = 0U; unit < fs.kib_per_block; ++unit) {
            u8 chunk[kib_size];
            if (!fs.read_kib(fs.context, block_number * fs.kib_per_block + unit, chunk))
                return false;
            for (u32 byte = 0U; byte < kib_size; ++byte)
                destination[unit * kib_size + byte] = chunk[byte];
        }
        return true;
    }

    /*
     * Same read, into a genuinely u32-typed array. Kept separate from
     * read_block_bytes() rather than reinterpret_cast'ing its u8* result:
     * writing into an object through a byte pointer is well-defined
     * regardless of the object's real type, but reading it back as u32
     * through a POINTER that was reinterpret_cast from u8* would not be --
     * only accessing the array itself, declared as u32[], is.
     */
    [[nodiscard]] inline bool read_block_pointers(
        const filesystem& fs, u32 block_number,
        u32 (&destination)[max_pointers_per_block]) noexcept {
        return read_block_bytes(fs, block_number, reinterpret_cast<u8*>(destination));
    }

    [[nodiscard]] inline bool mount(filesystem& fs, read_kib_fn read_kib, void* context) noexcept {
        fs.read_kib = read_kib;
        fs.context = context;
        u8 raw[kib_size];
        // The superblock is always the second 1024-byte unit, whatever the
        // filesystem's block size turns out to be -- this is the one read
        // in the whole reader that happens before block_size is known.
        if (!read_kib(context, 1U, raw))
            return false;
        static_assert(sizeof(superblock) <= kib_size);
        for (usize_t index = 0U; index < sizeof(superblock); ++index)
            reinterpret_cast<u8*>(&fs.sb)[index] = raw[index];

        if (fs.sb.magic != super_magic)
            return false;
        // Revision 0 has no inode_size or feature fields at all -- reading
        // them would read whatever garbage follows s_def_resgid on that
        // revision, not absent-but-zero values, so it is rejected outright
        // rather than guessed at.
        if (fs.sb.revision_level != revision_dynamic)
            return false;
        if ((fs.sb.feature_incompat & ~feature_incompat_supported_mask) != 0U)
            return false;
        if (fs.sb.log_block_size > 2U) // block size > 4096
            return false;
        fs.block_size = kib_size << fs.sb.log_block_size;
        fs.kib_per_block = fs.block_size / kib_size;
        fs.pointers_per_block = fs.block_size / sizeof(u32);
        if (fs.sb.inode_size < sizeof(inode) || fs.sb.inode_size > fs.block_size)
            return false;
        if (fs.sb.inodes_per_group == 0U || fs.sb.blocks_per_group == 0U)
            return false;
        if (fs.sb.blocks_count == 0U || fs.sb.inodes_count == 0U)
            return false;
        if (fs.sb.first_data_block >= fs.sb.blocks_count)
            return false;
        const u32 data_blocks = fs.sb.blocks_count - fs.sb.first_data_block;
        fs.group_count = (data_blocks + fs.sb.blocks_per_group - 1U) / fs.sb.blocks_per_group;
        return fs.group_count != 0U;
    }

    [[nodiscard]] inline bool read_group_descriptor(const filesystem& fs, u32 group,
                                                    group_descriptor& out) noexcept {
        if (group >= fs.group_count)
            return false;
        const u32 descriptors_per_block = fs.block_size / sizeof(group_descriptor);
        if (descriptors_per_block == 0U)
            return false;
        const u32 block = fs.sb.first_data_block + 1U + group / descriptors_per_block;
        u8 buffer[max_block_size];
        if (!read_block_bytes(fs, block, buffer))
            return false;
        const u32 offset = (group % descriptors_per_block) * sizeof(group_descriptor);
        for (usize_t index = 0U; index < sizeof(group_descriptor); ++index)
            reinterpret_cast<u8*>(&out)[index] = buffer[offset + index];
        return true;
    }

    [[nodiscard]] inline bool read_inode(const filesystem& fs, u32 inode_number,
                                        inode& out) noexcept {
        if (inode_number == 0U || inode_number > fs.sb.inodes_count)
            return false;
        const u32 index = inode_number - 1U;
        const u32 group = index / fs.sb.inodes_per_group;
        const u32 index_in_group = index % fs.sb.inodes_per_group;
        group_descriptor descriptor{};
        if (!read_group_descriptor(fs, group, descriptor))
            return false;
        const u64 byte_offset = static_cast<u64>(index_in_group) * fs.sb.inode_size;
        const u32 block_offset = static_cast<u32>(byte_offset / fs.block_size);
        const u32 offset_in_block = static_cast<u32>(byte_offset % fs.block_size);
        // ext2 guarantees inode_size divides block_size, so a real
        // filesystem never reaches this; it is checked anyway rather than
        // trusted, since the value came from untrusted on-disk input.
        if (offset_in_block + sizeof(inode) > fs.block_size)
            return false;
        u8 buffer[max_block_size];
        if (!read_block_bytes(fs, descriptor.inode_table + block_offset, buffer))
            return false;
        for (usize_t byte = 0U; byte < sizeof(inode); ++byte)
            reinterpret_cast<u8*>(&out)[byte] = buffer[offset_in_block + byte];
        return true;
    }

    enum class block_lookup {
        present,     // physical names a real block
        hole,        // sparse: no block was ever allocated here, reads as zero
        unsupported, // triple indirect; this reader's one real size limit
        error,       // malformed on-disk structure
    };

    /*
     * Resolves the physical block backing logical block `index` of a file,
     * walking direct, single-indirect, and double-indirect pointers.
     * Triple indirect is the one depth this reader does not implement --
     * files needing it are far larger than anything a boot disk holds here
     * -- and is reported as `unsupported` rather than silently truncated,
     * so a caller can distinguish "this file is too large for this reader"
     * from "this file legitimately ends here".
     */
    [[nodiscard]] inline block_lookup logical_block(const filesystem& fs, const inode& node,
                                                    u32 index, u32& physical) noexcept {
        physical = 0U;
        if (index < direct_block_count) {
            physical = node.block[index];
            return physical == 0U ? block_lookup::hole : block_lookup::present;
        }
        index -= direct_block_count;
        const u32 pointers = fs.pointers_per_block;

        if (index < pointers) {
            if (node.block[12] == 0U)
                return block_lookup::hole;
            u32 level1[max_pointers_per_block];
            if (!read_block_pointers(fs, node.block[12], level1))
                return block_lookup::error;
            physical = level1[index];
            return physical == 0U ? block_lookup::hole : block_lookup::present;
        }
        index -= pointers;

        if (index < pointers * pointers) {
            if (node.block[13] == 0U)
                return block_lookup::hole;
            u32 level1[max_pointers_per_block];
            if (!read_block_pointers(fs, node.block[13], level1))
                return block_lookup::error;
            const u32 outer = index / pointers;
            const u32 inner = index % pointers;
            if (level1[outer] == 0U)
                return block_lookup::hole;
            u32 level2[max_pointers_per_block];
            if (!read_block_pointers(fs, level1[outer], level2))
                return block_lookup::error;
            physical = level2[inner];
            return physical == 0U ? block_lookup::hole : block_lookup::present;
        }
        return block_lookup::unsupported;
    }

    /*
     * Reads up to `length` bytes starting at `offset` into `destination`,
     * returning the number of bytes actually produced -- which is less than
     * requested at end of file, and stops early (without failing what was
     * already read) on a hole-lookup error or an unsupported triple
     * indirect block.
     */
    [[nodiscard]] inline usize_t read_file(const filesystem& fs, const inode& node, u64 offset,
                                           u8* destination, usize_t length) noexcept {
        const u64 file_size = node.size;
        if (offset >= file_size)
            return 0U;
        const u64 remaining_in_file = file_size - offset;
        const usize_t to_read =
            static_cast<u64>(length) < remaining_in_file ? length : static_cast<usize_t>(remaining_in_file);

        usize_t produced = 0U;
        u8 block_buffer[max_block_size];
        while (produced < to_read) {
            const u64 position = offset + produced;
            const u32 logical = static_cast<u32>(position / fs.block_size);
            const u32 in_block = static_cast<u32>(position % fs.block_size);
            u32 physical = 0U;
            const block_lookup lookup = logical_block(fs, node, logical, physical);
            if (lookup == block_lookup::error || lookup == block_lookup::unsupported)
                break;

            usize_t chunk = fs.block_size - in_block;
            if (chunk > to_read - produced)
                chunk = to_read - produced;

            if (lookup == block_lookup::hole) {
                for (usize_t index = 0U; index < chunk; ++index)
                    destination[produced + index] = 0U;
            } else {
                if (!read_block_bytes(fs, physical, block_buffer))
                    break;
                for (usize_t index = 0U; index < chunk; ++index)
                    destination[produced + index] = block_buffer[in_block + index];
            }
            produced += chunk;
        }
        return produced;
    }

    /* Returning false from the visitor stops iteration early -- used by
     * path resolution to stop at the first name match. */
    using dirent_visitor_fn = bool (*)(void* context, const char* name, u8 name_length,
                                       u32 inode_number, u8 file_type) noexcept;

    [[nodiscard]] inline bool list_directory(const filesystem& fs, const inode& node,
                                             dirent_visitor_fn visitor, void* context) noexcept {
        if ((node.mode & mode_type_mask) != mode_directory)
            return false;
        const u32 block_count =
            static_cast<u32>((static_cast<u64>(node.size) + fs.block_size - 1U) / fs.block_size);
        u8 buffer[max_block_size];
        for (u32 logical = 0U; logical < block_count; ++logical) {
            u32 physical = 0U;
            const block_lookup lookup = logical_block(fs, node, logical, physical);
            if (lookup == block_lookup::error || lookup == block_lookup::unsupported)
                return false;
            if (lookup == block_lookup::hole)
                continue; // an unallocated directory block holds no entries
            if (!read_block_bytes(fs, physical, buffer))
                return false;

            u32 offset = 0U;
            while (offset + sizeof(directory_entry_header) <= fs.block_size) {
                directory_entry_header header{};
                for (usize_t byte = 0U; byte < sizeof(header); ++byte)
                    reinterpret_cast<u8*>(&header)[byte] = buffer[offset + byte];
                // A record_length that cannot fit is a corrupt block, not a
                // recoverable condition -- stop this block rather than walk
                // past its end on a bad stride.
                if (header.record_length < sizeof(header) ||
                    offset + header.record_length > fs.block_size)
                    break;
                if (header.inode != 0U && header.name_length != 0U) {
                    if (offset + sizeof(header) + header.name_length > fs.block_size)
                        break;
                    const auto* name = reinterpret_cast<const char*>(buffer + offset + sizeof(header));
                    if (!visitor(context, name, header.name_length, header.inode, header.file_type))
                        return true;
                }
                offset += header.record_length;
            }
        }
        return true;
    }

    namespace detail
    {
        struct lookup_state final {
            const char* name;
            usize_t name_length;
            u32 found_inode;
            bool found;
        };

        [[nodiscard]] inline bool lookup_visitor(void* context, const char* name, u8 name_length,
                                                 u32 inode_number, u8) noexcept {
            auto& state = *static_cast<lookup_state*>(context);
            if (static_cast<usize_t>(name_length) == state.name_length &&
                memcmp(name, state.name, state.name_length) == 0) {
                state.found_inode = inode_number;
                state.found = true;
                return false; // stop: found it
            }
            return true;
        }
    } // namespace detail

    /*
     * Resolves an absolute path to its inode number and content, one path
     * component at a time from the root inode. Not recursive: the walk is
     * a loop over the path string, bounded by its length, so there is no
     * stack-depth concern from a deeply nested path.
     */
    [[nodiscard]] inline bool resolve_path(const filesystem& fs, const char* path,
                                           u32& inode_number, inode& out) noexcept {
        if (path[0] != '/')
            return false;
        inode_number = root_inode_number;
        if (!read_inode(fs, inode_number, out))
            return false;

        usize_t cursor = 1U;
        while (path[cursor] != '\0') {
            const usize_t start = cursor;
            while (path[cursor] != '\0' && path[cursor] != '/')
                ++cursor;
            const usize_t length = cursor - start;
            if (path[cursor] == '/')
                ++cursor;
            if (length == 0U) // collapses a doubled '/' rather than erroring
                continue;
            if (length > max_name_length)
                return false;
            if ((out.mode & mode_type_mask) != mode_directory)
                return false; // a non-final component must be a directory

            detail::lookup_state state{path + start, length, 0U, false};
            if (!list_directory(fs, out, &detail::lookup_visitor, &state))
                return false;
            if (!state.found)
                return false;
            inode_number = state.found_inode;
            if (!read_inode(fs, inode_number, out))
                return false;
        }
        return true;
    }
} // namespace sys::ext2
