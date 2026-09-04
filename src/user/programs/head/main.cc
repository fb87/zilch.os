#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace
{
    [[nodiscard]] bool emit_lines(int descriptor, long limit) noexcept {
        char buffer[1];
        long lines = 0;
        while (lines < limit) {
            const ssize_t got = read(descriptor, buffer, 1U);
            if (got < 0)
                return false;
            if (got == 0)
                return true;
            (void)write(STDOUT_FILENO, buffer, 1U);
            if (buffer[0] == '\n')
                ++lines;
        }
        return true;
    }
} // namespace

extern "C" int sys_user_main(int argc, char** argv, char**) noexcept {
    long limit = 10;
    int file_index = 1;
    if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
        limit = atoi(argv[2]);
        file_index = 3;
    }

    if (file_index >= argc)
        return emit_lines(STDIN_FILENO, limit) ? 0 : 1;

    const int fd = open(argv[file_index], O_RDONLY);
    if (fd < 0)
        return 1;
    const bool ok = emit_lines(fd, limit);
    (void)close(fd);
    return ok ? 0 : 1;
}
