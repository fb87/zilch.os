#pragma once
#include <sys/arch/v1/types.hpp>
#include <sys/arch/v1/version.hpp>

namespace sys::arch::v1
{
    struct CpuOps final {
        cpu_id_t (*current_cpu)() noexcept;
        void (*relax)() noexcept;
        void (*halt)() noexcept;
    };

    struct IrqOps final {
        word_t (*save_disable)() noexcept;
        void (*restore)(word_t) noexcept;
    };

    struct MmuOps final {
        Error (*initialize)() noexcept;
        void (*invalidate_all)() noexcept;
    };

    struct HypervisorOps final {
        Error (*initialize)() noexcept;
        Error (*create_stage2)(vm_id_t, paddr_t*) noexcept;
        Error (*run)(vm_id_t) noexcept;
    };

    struct ArchOps final {
        u16 major, minor, patch, size;
        Capabilities capabilities;
        CpuOps cpu;
        IrqOps irq;
        MmuOps mmu;
        HypervisorOps hypervisor;
    };

    [[nodiscard]] const ArchOps& arch_ops() noexcept;
} // namespace sys::arch::v1
