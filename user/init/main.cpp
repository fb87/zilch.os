#include <sys/types.hpp>

extern "C" int main() noexcept {
    // Initial root task. Service discovery, capability distribution, and
    // server startup are introduced after the kernel can load a user ELF.
    return 0;
}
