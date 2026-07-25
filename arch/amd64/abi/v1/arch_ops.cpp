#include <sys/arch/v1/arch_ops.h>

namespace sys::arch::v1
{
    namespace
    {
        cpu_id_t current_cpu() noexcept {
            return 0U;
        }

        void relax() noexcept {
            __asm__ volatile("pause" ::: "memory");
        }

        [[noreturn]] void halt() noexcept {
            for (;;) {
                __asm__ volatile("cli; hlt");
            }
        }

        word_t save_disable() noexcept {
            word_t f;
            __asm__ volatile("pushfq; popq %0; cli" : "=r"(f)::"memory");
            return f;
        }

        void restore(word_t f) noexcept {
            __asm__ volatile("pushq %0; popfq" ::"r"(f) : "memory", "cc");
        }

        Error unsupported_init() noexcept {
            return Error::unsupported;
        }

        Error unsupported_stage2(vm_id_t, paddr_t*) noexcept {
            return Error::unsupported;
        }

        Error unsupported_run(vm_id_t) noexcept {
            return Error::unsupported;
        }

        void tlb_all() noexcept {
            word_t cr3;
            __asm__ volatile("mov %%cr3,%0; mov %0,%%cr3" : "=r"(cr3)::"memory");
        }

        const ArchOps ops{1,
                          0,
                          0,
                          sizeof(ArchOps),
                          {static_cast<u64>(Feature::smp) | static_cast<u64>(Feature::user) |
                               static_cast<u64>(Feature::virtualization) |
                               static_cast<u64>(Feature::stage2),
                           64,
                           48,
                           52,
                           0},
                          {current_cpu, relax, halt},
                          {save_disable, restore},
                          {unsupported_init, tlb_all},
                          {unsupported_init, unsupported_stage2, unsupported_run}};
    } // namespace

    const ArchOps& arch_ops() noexcept {
        return ops;
    }
} // namespace sys::arch::v1
