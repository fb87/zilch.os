#include <sys/arch/arch.hh>
#include <sys/arch/exception.hh>
#include <sys/kernel/printk.hh>

extern "C" void sys_amd64_exception_handler(sys::arch::exception::frame_t* frame) noexcept {
    pr_info("[EXCEPTION] vector=%llu error_code=%llu rip=%llx\n", frame->vector,
            frame->error_code, frame->instruction_pointer);

    if (frame->vector == 14U) {
        sys::u64 fault_addr = sys::arch::exception::fault_address();
        pr_info("[PF] address=%llx\n", fault_addr);
    }

    __asm__ volatile("cli; hlt");
    for (;;);
}

extern "C" void sys_arch_link_anchor() noexcept {}
