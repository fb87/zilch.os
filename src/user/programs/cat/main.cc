#include <unistd.h>

namespace
{
    inline constexpr size_t chunk_size = 512U;

    [[nodiscard]] bool stream(int descriptor) noexcept {
        char buffer[chunk_size];
        for (;;) {
            const ssize_t got = read(descriptor, buffer, sizeof(buffer));
            if (got < 0)
                return false;
            if (got == 0)
                return true;
            size_t written = 0U;
            while (written < static_cast<size_t>(got)) {
                const ssize_t put =
                    write(STDOUT_FILENO, buffer + written, static_cast<size_t>(got) - written);
                if (put <= 0)
                    return false;
                written += static_cast<size_t>(put);
            }
        }
    }
} // namespace

extern "C" int sys_user_main(int argc, char** argv, char**) noexcept {
    if (argc <= 1)
        return stream(STDIN_FILENO) ? 0 : 1;

    int status = 0;
    for (int index = 1; index < argc; ++index) {
        const int fd = open(argv[index], O_RDONLY);
        if (fd < 0) {
            status = 1;
            continue;
        }
        if (!stream(fd))
            status = 1;
        (void)close(fd);
    }
    return status;
}
