#include <string.h>
#include <unistd.h>

/*
 * The shell's own `echo` builtin shadows this for interactive use; this
 * exists so a pipeline stage or a script line naming `echo` explicitly
 * still resolves to a real, execv()-reachable binary (see sh/main.cc's
 * run_external(), which always execs rather than checking builtins for
 * anything past the first pipeline stage).
 */
extern "C" int sys_user_main(int argc, char** argv, char**) noexcept {
    for (int index = 1; index < argc; ++index) {
        if (index > 1)
            (void)write(STDOUT_FILENO, " ", 1U);
        (void)write(STDOUT_FILENO, argv[index], strlen(argv[index]));
    }
    (void)write(STDOUT_FILENO, "\n", 1U);
    return 0;
}
