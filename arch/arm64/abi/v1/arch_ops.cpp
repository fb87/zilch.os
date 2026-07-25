#include <sys/arch/v1/arch_ops.hpp>

namespace sys::arch::v1
{
    namespace
    {
        cpu_id_t current_cpu() noexcept {
            u64 value;
            __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
            return static_cast<cpu_id_t>(value & 0xffU);
        }

        void relax() noexcept {
            __asm__ volatile("yield" ::: "memory");
        }

        [[noreturn]] void halt() noexcept {
            for (;;) {
                __asm__ volatile("wfe");
            }
        }

        word_t save_disable() noexcept {
            word_t v;
            __asm__ volatile("mrs %0, daif\nmsr daifset, #0xf" : "=r"(v)::"memory");
            return v;
        }

        void restore(word_t v) noexcept {
            __asm__ volatile("msr daif, %0" ::"r"(v) : "memory");
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
            __asm__ volatile("dsb ishst\ntlbi vmalle1is\ndsb ish\nisb" ::: "memory");
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
                           48,
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
