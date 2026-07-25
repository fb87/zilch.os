#include <sys/arch/arch.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/timer.hh>

extern "C" void sys_arch_link_anchor() noexcept {}

extern "C" void sys_arm64_exception_handler(
    sys::arch::exception::frame_t* frame,
    sys::u64 level) noexcept
{
    const sys::u64 vector = frame->vector;
    if ((vector & 0x3U) == 1U) {
        const sys::irq_id_t irq = sys::platform::interrupt::acknowledge();
        if (irq == sys::platform::interrupt::virtual_timer_irq) {
            const sys::u64 ticks = sys::platform::timer::handle_interrupt();
            sys::kernel::scheduler::on_timer_tick();
            if (ticks == 1U && sys::arch::cpu::current_id() == 0U) {
                pr_info("timer interrupt active cpu=%u\n",
                        static_cast<unsigned int>(sys::arch::cpu::current_id()));
            }
        } else if (irq == sys::platform::interrupt::reschedule_ipi) {
            sys::arch::smp::record_reschedule_ipi();
            sys::kernel::scheduler::on_reschedule_ipi();
        } else if (irq == sys::platform::interrupt::tlb_shootdown_ipi) {
            sys::arch::smp::record_tlb_shootdown_ipi();
        }
        if (irq < 1020U) {
            sys::platform::interrupt::complete(irq);
        }
        return;
    }

    sys::arch::irq::disable();
    pr_err("exception el=%llu vector=%llu esr=%llx far=%llx elr=%llx\n",
           static_cast<unsigned long long>(level),
           static_cast<unsigned long long>(vector),
           static_cast<unsigned long long>(sys::arch::exception::syndrome(static_cast<sys::u32>(level))),
           static_cast<unsigned long long>(sys::arch::exception::fault_address(static_cast<sys::u32>(level))),
           static_cast<unsigned long long>(sys::arch::exception::return_address(static_cast<sys::u32>(level))));
    sys::arch::cpu::halt();
}
