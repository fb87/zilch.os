#include <sys/arch/arch.hh>
#include <sys/arch/exception.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/kernel/syscall/control.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/timer.hh>

extern "C" void sys_amd64_exception_handler(sys::arch::exception::frame_t* frame) noexcept {
    const sys::u64 vector = frame->vector;

    if (vector == 32U) {
        /* Timer interrupt (vector 32 = IRQ0 mapped by IOAPIC) */
        const sys::kernel::interrupt::timing::latency_scope preemption_latency{
            sys::kernel::interrupt::timing::latency_kind::preemption_service};
        const sys::u64 ticks = sys::platform::timer::handle_interrupt();
        if (sys::kernel::thread::user_execution_active[sys::arch::cpu::current_id()]) {
            sys::kernel::thread::schedule_user(*frame);
        } else {
            sys::kernel::scheduler::on_timer_tick();
        }
        sys::platform::interrupt::complete(32U);
        return;
    }

    /* All other exceptions are fatal in Phase 7 */
    pr_info("[EXCEPTION] vector=%llu error_code=%llu rip=%llx\n", vector,
            frame->error_code, frame->instruction_pointer);

    if (vector == 14U) {
        sys::u64 fault_addr = sys::arch::exception::fault_address();
        pr_info("[PF] address=%llx\n", fault_addr);
    }

    __asm__ volatile("cli; hlt");
    for (;;);
}

extern "C" void sys_arch_link_anchor() noexcept {}
