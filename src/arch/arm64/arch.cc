#include <sys/arch/arch.hh>
#include <sys/arch/stack.hh>
#include <sys/kernel/emergency.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/kernel/syscall/control.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/timer.hh>

extern "C" void sys_arch_link_anchor() noexcept {}

extern "C" void sys_arm64_exception_handler(sys::arch::exception::frame_t* frame,
                                            sys::u64 level) noexcept {
    if (!sys::arch::stack::observe(level)) {
        sys::arch::irq::disable();
        sys::kernel::emergency::append(sys::kernel::emergency::event::stack_corruption, level);
        sys::kernel::emergency::preserve(static_cast<sys::u32>(level), frame->vector, 0U, 0U,
                                         frame->instruction_pointer);
        pr_err("kernel stack overflow/corruption cpu=%u el=%llu\n",
               static_cast<unsigned int>(sys::arch::cpu::current_id()),
               static_cast<unsigned long long>(level));
        sys::arch::cpu::halt();
    }
    const sys::kernel::object::read_guard object_read_guard{};
    const sys::u64 vector = frame->vector;
    const sys::u64 syndrome = sys::arch::exception::syndrome(static_cast<sys::u32>(level));
    sys::kernel::emergency::append(sys::kernel::emergency::event::exception_entry, level, vector,
                                   syndrome, frame->instruction_pointer);

    /*
     * ESR_ELx is meaningful only for synchronous exceptions.  Dispatch by
     * vector class first so an IRQ cannot be mistaken for the preceding SVC
     * because ESR_EL1 retained an old syndrome value.
     */
    const sys::u64 exception_class = vector & 0x3U;

    if (exception_class == 1U) {
        const sys::irq_id_t irq = sys::platform::interrupt::acknowledge();
        if (irq == sys::platform::interrupt::virtual_timer_irq) {
            const sys::u64 ticks = sys::platform::timer::handle_interrupt();
            if (sys::kernel::thread::user_execution_active[sys::arch::cpu::current_id()]) {
                if (vector == 9U) {
                    sys::kernel::thread::schedule_user(*frame);
                } else if (vector == 5U) {
                    (void)sys::kernel::thread::resume_user_from_idle(*frame);
                }
            } else {
                sys::kernel::scheduler::on_timer_tick();
            }
            if (ticks == 1U && sys::arch::cpu::current_id() == 0U) {
                pr_info("timer interrupt active cpu=%u\n",
                        static_cast<unsigned int>(sys::arch::cpu::current_id()));
            }
        } else if (irq == sys::platform::interrupt::reschedule_ipi) {
            sys::arch::smp::record_reschedule_ipi();
            if (sys::kernel::thread::user_execution_active[sys::arch::cpu::current_id()]) {
                if (vector == 9U) {
                    sys::kernel::thread::schedule_user(*frame);
                } else if (vector == 5U) {
                    (void)sys::kernel::thread::resume_user_from_idle(*frame);
                }
            } else {
                sys::kernel::scheduler::on_reschedule_ipi();
            }
        } else if (irq == sys::platform::interrupt::tlb_shootdown_ipi) {
            sys::arch::memory::invalidate_tlb_all();
            sys::arch::smp::record_tlb_shootdown_ipi();
        }
        if (irq < 1020U) {
            sys::platform::interrupt::complete(irq);
        }
        return;
    }

    if (exception_class == 0U) {
        if (level == 2U && sys::arch::hypervisor::dispatch(*frame, syndrome)) {
            return;
        }

        /* EL0 AArch64 synchronous exceptions enter EL1 through vector 8. */
        if (level == 1U && sys::kernel::syscall::dispatch_control(sys::kernel::thread::current(),
                                                                  *frame, vector, syndrome)) {
            return;
        }
        if (level == 1U && sys::kernel::syscall::dispatch_ipc(sys::kernel::thread::current(),
                                                              *frame, vector, syndrome)) {
            return;
        }
        if (level == 1U && sys::kernel::thread::handle_user_fault(
                               *frame, vector, syndrome, sys::arch::exception::fault_address(1U))) {
            return;
        }
    }

    sys::arch::irq::disable();
    const sys::u64 fault_address =
        sys::arch::exception::fault_address(static_cast<sys::u32>(level));
    sys::kernel::emergency::append(sys::kernel::emergency::event::fatal_exception, level, vector,
                                   syndrome, fault_address, frame->instruction_pointer);
    sys::kernel::emergency::preserve(static_cast<sys::u32>(level), vector, syndrome, fault_address,
                                     frame->instruction_pointer);
    pr_err("exception el=%llu vector=%llu esr=%llx far=%llx elr=%llx\n",
           static_cast<unsigned long long>(level), static_cast<unsigned long long>(vector),
           static_cast<unsigned long long>(syndrome),
           static_cast<unsigned long long>(fault_address),
           static_cast<unsigned long long>(frame->instruction_pointer));
    sys::arch::cpu::halt();
}
