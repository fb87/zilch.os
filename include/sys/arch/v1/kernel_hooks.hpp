#pragma once
#include <sys/arch/v1/types.hpp>

namespace sys::arch::v1
{
    enum class ExceptionClass : u16 {
        unknown,
        syscall,
        instruction_fault,
        data_fault,
        interrupt,
        virtualization
    };

    struct ExceptionInfo final {
        ExceptionClass type;
        word_t syndrome;
        vaddr_t fault_address;
    };

    struct KernelHooks final {
        void (*exception)(UserContext*, const ExceptionInfo*) noexcept;
        void (*interrupt)(UserContext*, irq_id_t) noexcept;
        void (*fatal)(const char*) noexcept;
    };

    [[nodiscard]] const KernelHooks& kernel_hooks() noexcept;
} // namespace sys::arch::v1
