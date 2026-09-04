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
    /*
     * The fixed virtual address a process's argument block lives at,
     * shared between userspace (native.hh's args_address, which just
     * aliases this) and the kernel (exec_user_image(), which needs to name
     * this address directly to snapshot the block across exec's address-
     * space teardown -- see that function for why). One name in one place
     * rather than two constants that happen to agree.
     */
    inline constexpr word_t process_args_address = 0x20054000U;
    /* Matches native.hh's args_frame: the well-known cspace selector a
     * process's argument-block capability lives at. Shared for the same
     * reason process_args_address is -- exec_user_image() needs to name
     * this selector directly to restore the block after reclaim discards
     * whatever frame it named beforehand. */
    inline constexpr word_t process_args_frame_selector = 16U;

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
    /*
     * Encodes argv/envp into a block at `base_address`, which the caller has
     * already mapped writable. Shared by root's spawn() (which maps a
     * frame it is about to mint away) and libc's execv() (which writes
     * directly at the address a process's own args frame is already
     * mapped at, inherited across fork -- two different callers with two
     * different reasons the memory is there, but identical encoding once
     * it is), so the format has exactly one place it can drift from
     * valid_process_args()'s own expectations.
     *
     * Returns false, without partially completing, if the block does not
     * fit -- too many entries or too many bytes -- so a caller can report
     * "argument list too long" rather than execing with a silently
     * truncated argv.
     */
    [[nodiscard]] inline bool encode_process_args(void* base_address, const char* const* argv,
                                                  word_t argc, const char* const* envp,
                                                  word_t envc) noexcept {
        if (argc + envc > process_args_max_entries)
            return false;
        auto* const header = reinterpret_cast<process_args_header*>(base_address);
        auto* const base = reinterpret_cast<char*>(base_address);
        for (usize_t index = 0U; index < process_args_size; ++index)
            base[index] = '\0';

        header->magic = process_args_magic;
        header->version = process_args_version;
        header->argc = static_cast<u32>(argc);
        header->envc = static_cast<u32>(envc);
        header->bytes_offset = static_cast<u32>(sizeof(process_args_header));
        header->bytes_size = 0U;

        char* const bytes = base + header->bytes_offset;
        const u32 capacity = static_cast<u32>(process_args_size) - header->bytes_offset;
        u32 cursor = 0U;
        bool overflowed = false;
        const auto append = [&](const char* text, u32 slot) noexcept {
            header->entries[slot] = cursor;
            for (const char* character = text; *character != '\0'; ++character) {
                if (cursor >= capacity) {
                    overflowed = true;
                    return;
                }
                bytes[cursor++] = *character;
            }
            if (cursor >= capacity) {
                overflowed = true;
                return;
            }
            bytes[cursor++] = '\0';
        };
        for (word_t index = 0U; index < argc && !overflowed; ++index)
            append(argv[index], static_cast<u32>(index));
        for (word_t index = 0U; index < envc && !overflowed; ++index)
            append(envp[index], static_cast<u32>(argc + index));
        header->bytes_size = cursor;
        return !overflowed;
    }

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
