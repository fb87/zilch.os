#include <sys/arch/memory.hh>
#include <sys/arch/stack.hh>
#include <sys/kernel/kernel.hh>

extern "C" [[noreturn]] void sys_kernel_entry(sys::uintptr_t firmware_data) noexcept {
    sys::arch::stack::initialize_current_cpu();

#if defined(__x86_64__)
    /* Phase 2 (amd64 only): Build the kernel's real page tables. For now, we keep using the
     * temporary tables from boot/start.S, but this verifies the table building logic works.
     * In later phases, we'll switch to using these real tables.
     */
    sys::arch::memory::build_kernel_table(sys::arch::memory::kernel_pml4,
                                          sys::arch::memory::kernel_pdpt,
                                          sys::arch::memory::kernel_pd);
#endif

    sys::kernel::start(firmware_data);
}

extern "C" [[noreturn]] void sys_kernel_secondary_entry() noexcept {
    sys::arch::stack::initialize_current_cpu();
    sys::kernel::start_secondary();
}

extern "C" [[noreturn]] void sys_kernel_user_idle() noexcept {
    for (;;) {
        sys::arch::cpu::wait_for_event();
    }
}
