#pragma once

#include <sys/types.hh>

/*
 * How a process receives arguments.
 *
 * A user thread starts with exactly two machine words in x0/x1 (see
 * arch::context::initialize_user), which was enough while every program was
 * a server whose only "argument" was its role number. A shell has to hand a
 * command its argv and environment, so the second word now optionally
 * carries the address of a page the spawner mapped into the new process.
 *
 * The block is a flat, self-contained image: offsets rather than pointers,
 * so the spawner does not need to know where the page will land in the
 * child, and one bounds check on `bytes_size` covers every string. It is
 * deliberately not a pointer array in the seL4/Linux auxv style -- there is
 * no loader here to relocate one, and USR-015 deferred auxv entirely for
 * want of a consumer.
 *
 * Word 1 being zero means "no block", which is what every existing server
 * is started with and why they keep working unchanged.
 */
namespace sys::abi::v1
{
    inline constexpr u32 process_args_magic = 0x5a415247U; // "ZARG"
    inline constexpr u32 process_args_version = 1U;

    /* One page total, so a block is always exactly one frame to map. */
    inline constexpr usize_t process_args_size = 4096U;
    inline constexpr u32 process_args_max_entries = 64U;

    struct process_args_header {
        u32 magic;
        u32 version;
        u32 argc;
        u32 envc;
        /* Byte offset from the start of the block to the string area, and
         * its length. Every entry offset is relative to that area. */
        u32 bytes_offset;
        u32 bytes_size;
        u32 reserved0;
        u32 reserved1;
        /* argc entries, then envc entries, each a byte offset into the
         * string area of a NUL-terminated string. */
        u32 entries[process_args_max_entries];
    };

    static_assert(sizeof(process_args_header) < process_args_size,
                  "the header and its entry table must leave room for strings");

    /*
     * Validates a block far enough that a reader can walk it without
     * trusting the writer. Callers still bounds-check each string's NUL,
     * because a truncated final string is the one thing offsets alone
     * cannot rule out.
     */
    [[nodiscard]] inline bool valid_process_args(const process_args_header& header) noexcept {
        if (header.magic != process_args_magic || header.version != process_args_version)
            return false;
        if (header.argc > process_args_max_entries ||
            header.envc > process_args_max_entries - header.argc)
            return false;
        if (header.bytes_offset < sizeof(process_args_header) ||
            header.bytes_offset >= process_args_size)
            return false;
        if (header.bytes_size > process_args_size - header.bytes_offset)
            return false;
        const u32 total = header.argc + header.envc;
        for (u32 index = 0U; index < total; ++index) {
            if (header.entries[index] >= header.bytes_size)
                return false;
        }
        return true;
    }
} // namespace sys::abi::v1
