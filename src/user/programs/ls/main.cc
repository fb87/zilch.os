#include <stdio.h>
#include <unistd.h>

/*
 * No cwd concept exists below the shell yet (see sh/main.cc's `cd`
 * builtin), so a bare `ls` lists "/" rather than a working directory that
 * does not exist as a real concept here.
 */
extern "C" int sys_user_main(int argc, char** argv, char**) noexcept {
    const char* path = argc > 1 ? argv[1] : "/";
    const int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 1;

    for (int index = 0;; ++index) {
        char name[64];
        bool is_directory = false;
        const int result = vfs_readdir(fd, index, name, sizeof(name), &is_directory);
        if (result < 0) {
            (void)close(fd);
            return 1;
        }
        if (result == 0)
            break;
        printf("%s%s\n", name, is_directory ? "/" : "");
    }
    (void)close(fd);
    return 0;
}
