#include <sys/arch/current.hpp>
#include <sys/arch/v1/kernel_hooks.hpp>

namespace sys::arch::v1
{
    namespace
    {
        void exception(UserContext*, const ExceptionInfo*) noexcept {}

        void interrupt(UserContext*, irq_id_t) noexcept {}

        void fatal(const char*) noexcept {
            arch_ops().cpu.halt();
            for (;;) {
            }
        }

        const KernelHooks hooks{exception, interrupt, fatal};
    } // namespace

    const KernelHooks& kernel_hooks() noexcept {
        return hooks;
    }
} // namespace sys::arch::v1
