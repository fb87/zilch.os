#pragma once

#include <sys/types.hh>

/*
 * ext2 on-disk format, read-only.
 *
 * This is an EXTERNAL format this system does not control -- unlike
 * abi/v1, which zilch defines and freezes itself, these layouts are fixed
 * by the ext2 specification and must match byte-for-byte whatever wrote the
 * disk image. Every struct is packed and offset-checked with
 * static_assert, the same discipline sys::vmm::elf uses for guest ELF
 * headers, for the same reason: this is untrusted external input, parsed
 * by an unprivileged userspace process, and a layout mistake here is a
 * memory-safety bug, not a cosmetic one.
 */
namespace sys::ext2
{
    inline constexpr u16 super_magic = 0xef53U;
    inline constexpr u32 super_block_offset = 1024U; // fixed, regardless of block size

    /*
     * Only fields this reader actually uses are declared. The struct is
     * still packed and contiguous from offset 0, so every declared field
     * sits at its true on-disk offset; fields after s_algorithm_usage_bitmap
     * (checksums, journal, resize inode, orphan list) are never read and so
     * are not declared, rather than padded out to the full 1024-byte
     * superblock for no benefit.
     */
    struct superblock final {
        u32 inodes_count;
        u32 blocks_count;
        u32 reserved_blocks_count;
        u32 free_blocks_count;
        u32 free_inodes_count;
        u32 first_data_block;
        u32 log_block_size; // block size = 1024 << log_block_size
        u32 log_fragment_size;
        u32 blocks_per_group;
        u32 fragments_per_group;
        u32 inodes_per_group;
        u32 mount_time;
        u32 write_time;
        u16 mount_count;
        u16 max_mount_count;
        u16 magic;
        u16 state;
        u16 errors;
        u16 minor_revision_level;
        u32 last_check;
        u32 check_interval;
        u32 creator_os;
        u32 revision_level;
        u16 default_reserved_uid;
        u16 default_reserved_gid;
        // EXT2_DYNAMIC_REV (revision_level == 1) fields; a revision-0
        // filesystem has none of these and this reader rejects it (see
        // valid()) rather than guess at absent values.
        u32 first_inode;
        u16 inode_size;
        u16 block_group_number;
        u32 feature_compat;
        u32 feature_incompat;
        u32 feature_ro_compat;
        u8 uuid[16];
        u8 volume_name[16];
        u8 last_mounted[64];
        u32 algorithm_usage_bitmap;
    } __attribute__((packed));

    static_assert(__builtin_offsetof(superblock, log_block_size) == 24U);
    static_assert(__builtin_offsetof(superblock, magic) == 56U);
    static_assert(__builtin_offsetof(superblock, revision_level) == 76U);
    static_assert(__builtin_offsetof(superblock, inode_size) == 88U);
    static_assert(__builtin_offsetof(superblock, feature_incompat) == 96U);
    static_assert(__builtin_offsetof(superblock, uuid) == 104U);

    inline constexpr u32 revision_dynamic = 1U;

    // The only incompat feature this reader understands. Any other bit
    // means a layout it cannot safely parse -- extents (ext4) replace
    // i_block's meaning entirely, 64bit widens group descriptors, and so
    // on -- so mount() rejects every incompat bit outside this mask rather
    // than enumerate every feature that would corrupt a read.
    inline constexpr u32 feature_incompat_filetype = 0x0002U;
    inline constexpr u32 feature_incompat_supported_mask = feature_incompat_filetype;

    struct group_descriptor final {
        u32 block_bitmap;
        u32 inode_bitmap;
        u32 inode_table;
        u16 free_blocks_count;
        u16 free_inodes_count;
        u16 used_dirs_count;
        u16 padding;
        u8 reserved[12];
    } __attribute__((packed));
    static_assert(sizeof(group_descriptor) == 32U);

    inline constexpr u32 root_inode_number = 2U;

    // Mode bits (i_mode's high nibble); only the two types this reader
    // resolves are named.
    inline constexpr u16 mode_type_mask = 0xf000U;
    inline constexpr u16 mode_regular = 0x8000U;
    inline constexpr u16 mode_directory = 0x4000U;

    inline constexpr u32 direct_block_count = 12U;
    inline constexpr u32 block_pointer_count = 15U; // 12 direct + single/double/triple indirect

    /*
     * The classic 128-byte inode layout. A filesystem's actual inode
     * stride is superblock::inode_size, which may be larger (256 is the
     * common modern default) -- read_inode() advances by that stride and
     * reads only this struct's worth of it, so the extended-attribute
     * space past byte 128 is simply never touched.
     */
    struct inode final {
        u16 mode;
        u16 uid;
        u32 size;
        u32 access_time;
        u32 create_time;
        u32 modify_time;
        u32 delete_time;
        u16 gid;
        u16 links_count;
        u32 blocks; // 512-byte units; unused by this reader, kept for layout
        u32 flags;
        u32 osd1;
        u32 block[block_pointer_count];
        u32 generation;
        u32 file_acl;
        u32 size_high; // high 32 bits for files > 4 GiB; not supported here
        u32 fragment_address;
        u8 osd2[12];
    } __attribute__((packed));
    static_assert(sizeof(inode) == 128U);
    static_assert(__builtin_offsetof(inode, size) == 4U);
    static_assert(__builtin_offsetof(inode, block) == 40U);

    inline constexpr u8 file_type_unknown = 0U;
    inline constexpr u8 file_type_regular = 1U;
    inline constexpr u8 file_type_directory = 2U;

    /*
     * Variable-length: name[] follows immediately and is name_length bytes,
     * NOT NUL-terminated. rec_len is the entry's total size including this
     * header and any trailing padding, which is what a reader advances by
     * -- never name_length alone, since a deleted entry's rec_len can span
     * more room than its current name occupies.
     */
    struct directory_entry_header final {
        u32 inode;
        u16 record_length;
        u8 name_length;
        u8 file_type;
    } __attribute__((packed));
    static_assert(sizeof(directory_entry_header) == 8U);

    inline constexpr usize_t max_name_length = 255U;
} // namespace sys::ext2
