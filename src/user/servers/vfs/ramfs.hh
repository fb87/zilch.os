#pragma once

#include <stdlib.h>
#include <string.h>

#include <sys/types.hh>

/*
 * The writable half of the VFS: a small, flat, in-memory filesystem for
 * /tmp. ext2 is mounted read-only for this pass (see ext2::reader), so
 * redirection and scratch files need somewhere writable, and /tmp is that
 * somewhere.
 *
 * Deliberately flat -- no subdirectories -- because nothing that needs /tmp
 * needs a hierarchy under it: a shell redirecting output or a pipeline's
 * intermediate file wants a name, not a tree. Storage rides on this
 * process's own malloc/realloc, which is itself frame-backed (see
 * lib/libc/malloc.cc), rather than hand-rolling frame management a second
 * time for what is, underneath, the same kind of growable buffer.
 */
namespace sys::vfs::ramfs
{
    inline constexpr int max_files = 16;
    inline constexpr usize_t max_name = 60U;

    struct file final {
        bool used{};
        char name[max_name]{};
        u8* data{};
        usize_t size{};
        usize_t capacity{};
    };

    inline file files[max_files]{};

    [[nodiscard]] inline int find(const char* name) noexcept {
        for (int index = 0; index < max_files; ++index) {
            if (files[index].used && strcmp(files[index].name, name) == 0)
                return index;
        }
        return -1;
    }

    [[nodiscard]] inline int create(const char* name) noexcept {
        const usize_t length = strlen(name);
        if (length == 0U || length >= max_name)
            return -1;
        const int existing = find(name);
        if (existing >= 0)
            return existing;
        for (int index = 0; index < max_files; ++index) {
            if (files[index].used)
                continue;
            files[index] = {};
            files[index].used = true;
            (void)strcpy(files[index].name, name);
            return index;
        }
        return -1;
    }

    [[nodiscard]] inline bool ensure_capacity(file& target, usize_t needed) noexcept {
        if (needed <= target.capacity)
            return true;
        // Grow geometrically rather than to exactly `needed`, so a stream
        // of small appends -- the common shell-redirection case -- does not
        // realloc on every single write.
        usize_t grown = target.capacity == 0U ? 256U : target.capacity;
        while (grown < needed)
            grown *= 2U;
        auto* replacement = static_cast<u8*>(realloc(target.data, grown));
        if (replacement == nullptr)
            return false;
        target.data = replacement;
        target.capacity = grown;
        return true;
    }

    /*
     * Writes at an arbitrary offset, zero-filling any gap between the
     * current size and `offset` -- the same sparse-write semantic a real
     * file gives a seek-then-write pattern. Returns the number of bytes
     * written, 0 on allocation failure.
     */
    [[nodiscard]] inline usize_t write_at(file& target, u64 offset, const u8* source,
                                         usize_t length) noexcept {
        if (offset > static_cast<u64>(static_cast<usize_t>(-1)) - length)
            return 0U; // offset + length would overflow usize_t
        const usize_t needed = static_cast<usize_t>(offset) + length;
        if (!ensure_capacity(target, needed))
            return 0U;
        if (static_cast<usize_t>(offset) > target.size)
            (void)memset(target.data + target.size, 0, static_cast<usize_t>(offset) - target.size);
        (void)memcpy(target.data + offset, source, length);
        if (needed > target.size)
            target.size = needed;
        return length;
    }

    [[nodiscard]] inline usize_t read_at(const file& target, u64 offset, u8* destination,
                                        usize_t length) noexcept {
        if (offset >= target.size)
            return 0U;
        const usize_t available = target.size - static_cast<usize_t>(offset);
        const usize_t chunk = length < available ? length : available;
        (void)memcpy(destination, target.data + offset, chunk);
        return chunk;
    }

    inline void truncate(file& target) noexcept {
        target.size = 0U;
    }
} // namespace sys::vfs::ramfs
