#include <sys/types.hh>

/*
 * Proves the argument path end to end from inside a real spawned process:
 * the role pool bound an image, root filled an argument frame and minted it,
 * this process mapped it and parsed it, the runtime dispatched to the POSIX
 * entry, and the exit status survives back to the spawner.
 *
 * It reports by exit status rather than by writing to the console, so it
 * needs no capability beyond the argument frame -- which keeps what is
 * under test to exactly the argument path. A distinct code per failure mode
 * means a regression says which link broke, instead of only that something
 * did.
 */
namespace
{
    [[nodiscard]] bool equal(const char* left, const char* right) noexcept {
        for (; *left != '\0' && *right != '\0'; ++left, ++right) {
            if (*left != *right)
                return false;
        }
        return *left == *right;
    }
} // namespace

extern "C" int sys_user_main(int argc, char** argv, char** envp) noexcept {
    if (argv == nullptr || envp == nullptr)
        return 11;
    if (argc != 3)
        return 12;
    if (!equal(argv[0], "argv-probe"))
        return 13;
    if (!equal(argv[1], "alpha"))
        return 14;
    if (!equal(argv[2], "beta"))
        return 15;
    /* The vectors must be NULL-terminated, not merely correct up to argc:
     * every consumer of argv walks to the terminator. */
    if (argv[3] != nullptr)
        return 16;
    if (envp[0] == nullptr || !equal(envp[0], "ZILCH=1"))
        return 17;
    if (envp[1] != nullptr)
        return 18;
    return 42;
}
