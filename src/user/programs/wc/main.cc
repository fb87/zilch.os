#include <stdio.h>
#include <unistd.h>

namespace
{
    struct counts final {
        long lines = 0;
        long words = 0;
        long bytes = 0;
    };

    [[nodiscard]] bool count_stream(int descriptor, counts& total) noexcept {
        char buffer[512];
        bool in_word = false;
        for (;;) {
            const ssize_t got = read(descriptor, buffer, sizeof(buffer));
            if (got < 0)
                return false;
            if (got == 0)
                return true;
            total.bytes += got;
            for (ssize_t index = 0; index < got; ++index) {
                const char value = buffer[index];
                if (value == '\n')
                    ++total.lines;
                const bool space = value == ' ' || value == '\t' || value == '\n' || value == '\r';
                if (space) {
                    in_word = false;
                } else if (!in_word) {
                    in_word = true;
                    ++total.words;
                }
            }
        }
    }
} // namespace

extern "C" int sys_user_main(int argc, char** argv, char**) noexcept {
    if (argc <= 1) {
        counts total;
        if (!count_stream(STDIN_FILENO, total))
            return 1;
        printf("%7ld %7ld %7ld\n", total.lines, total.words, total.bytes);
        return 0;
    }

    int status = 0;
    for (int index = 1; index < argc; ++index) {
        const int fd = open(argv[index], O_RDONLY);
        if (fd < 0) {
            status = 1;
            continue;
        }
        counts total;
        const bool ok = count_stream(fd, total);
        (void)close(fd);
        if (!ok) {
            status = 1;
            continue;
        }
        printf("%7ld %7ld %7ld %s\n", total.lines, total.words, total.bytes, argv[index]);
    }
    return status;
}
