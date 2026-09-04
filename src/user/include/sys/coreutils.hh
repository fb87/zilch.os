#pragma once

#include <sys/types.hh>

/*
 * The fixed set of programs a shell can exec by name.
 *
 * Loading an arbitrary program from the ext2 disk at runtime is out of
 * scope for this pass: role_image_bind is root-gated (deliberately -- a
 * process choosing what code runs under its own role is not something a
 * non-root task should get to do to itself), and every EXISTING loader
 * (image_for_role, process_exec) only ever resolves a role against the
 * statically linked earlyfs blob. Extending either to load arbitrary bytes
 * a process read from a file is real future work, not attempted here.
 *
 * What this DOES support fully: root binds each of these roles once, at
 * boot, exactly the way every other earlyfs-resident program's role is
 * bound -- and since process_exec only needs an ALREADY-bound role, not
 * the right to bind one, an ordinary forked child can exec any of them
 * without needing any privilege it does not already have. The shell's own
 * argv/envp still travel for real, through the same args-frame mechanism
 * every spawned program already uses (see abi/v1/process.hh's
 * encode_process_args()) -- only which BINARY loads is fixed in advance,
 * not what it is told to do once it runs.
 *
 * This is the one table both root_graph.hh (which binds these roles at
 * boot) and libc's execv() (which resolves a name against them) share, so
 * the two can never drift out of step with each other.
 */
namespace sys::coreutils
{
    struct entry final {
        const char* name;
        word_t role;
        const char* image;
    };

    inline constexpr word_t role_base = 0x500U;

    inline constexpr entry table[] = {
        {"sh", role_base + 0U, "bin/sh"},
        {"ls", role_base + 1U, "bin/ls"},
        {"cat", role_base + 2U, "bin/cat"},
        {"echo", role_base + 3U, "bin/echo"},
        {"true", role_base + 4U, "bin/true"},
        {"false", role_base + 5U, "bin/false"},
        {"wc", role_base + 6U, "bin/wc"},
        {"head", role_base + 7U, "bin/head"},
    };
    inline constexpr usize_t table_count = sizeof(table) / sizeof(table[0]);

    /*
     * Matches by basename: a caller may exec "/bin/ls" or just "ls" and get
     * the same role, since there is no real path hierarchy behind these --
     * every one of them is a fixed earlyfs-resident image, not a file the
     * disk could actually contain a different version of at another path.
     */
    [[nodiscard]] inline const char* basename_of(const char* path) noexcept {
        const char* last = path;
        for (const char* cursor = path; *cursor != '\0'; ++cursor) {
            if (*cursor == '/')
                last = cursor + 1;
        }
        return last;
    }

    [[nodiscard]] inline bool resolve(const char* path, word_t& role) noexcept {
        const char* name = basename_of(path);
        for (usize_t index = 0U; index < table_count; ++index) {
            const char* left = name;
            const char* right = table[index].name;
            while (*left != '\0' && *left == *right) {
                ++left;
                ++right;
            }
            if (*left == '\0' && *right == '\0') {
                role = table[index].role;
                return true;
            }
        }
        return false;
    }
} // namespace sys::coreutils
