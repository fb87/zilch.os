#include <sys/platform/v1/platform_ops.hpp>

namespace sys::platform::v1
{
    namespace
    {
        constexpr uintptr_t uart_base = 0x09000000ULL;

        void init() noexcept {}

        void putc(char c) noexcept {
            auto* dr = reinterpret_cast<volatile u32*>(uart_base);
            auto* fr = reinterpret_cast<volatile u32*>(uart_base + 0x18U);
            while ((*fr & (1U << 5U)) != 0U) {
            }
            *dr = static_cast<u32>(static_cast<unsigned char>(c));
        }

        Error irq_init() noexcept {
            return Error::unsupported;
        }

        irq_id_t ack() noexcept {
            return 1023U;
        }

        void complete(irq_id_t) noexcept {}

        const BootInfo info{nullptr, 0, 0, 0, 1};

        const BootInfo* boot() noexcept {
            return &info;
        }

        const PlatformOps ops{1,
                              0,
                              0,
                              sizeof(PlatformOps),
                              "qemu-arm64-virt",
                              {init, putc},
                              {irq_init, ack, complete},
                              boot};
    } // namespace

    const PlatformOps& platform_ops() noexcept {
        return ops;
    }
} // namespace sys::platform::v1
