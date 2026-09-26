#include <sys/arch/arch.hh>
#include <sys/arch/stack.hh>
#include <sys/kernel/emergency.hh>
#include <sys/kernel/interrupt.hh>
#include <sys/kernel/object/table.hh>
#include <sys/kernel/panic.hh>
#include <sys/kernel/printk.hh>
#include <sys/kernel/scheduler.hh>
#include <sys/kernel/syscall/control.hh>
#include <sys/kernel/syscall/ipc.hh>
#include <sys/kernel/thread/scheduler.hh>
#include <sys/platform/interrupt.hh>
#include <sys/platform/timer.hh>
#if CONFIG_TESTS
#include <sys/kernel/tests/ipc/lifecycle_race.hh>
#include <sys/kernel/tests/memory/allocator_race.hh>
#include <sys/kernel/tests/memory/revoke_race.hh>
#endif

extern "C" void sys_arch_link_anchor() noexcept {}

extern "C" void sys_arm64_exception_handler(sys::arch::exception::frame_t* frame,
                                            sys::u64 level) noexcept {
    if (!sys::arch::stack::observe(level)) {
        sys::kernel::panic::stop(sys::kernel::panic::reason::stack_corruption,
                                 static_cast<sys::u32>(level), frame->vector, 0U, 0U,
                                 frame->instruction_pointer);
    }
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
        bool userspace_deactivate = false;
        {
            const sys::kernel::interrupt::timing::latency_scope interrupt_latency{
                sys::kernel::interrupt::timing::latency_kind::interrupt_service};
            sys::kernel::emergency::trace(sys::kernel::emergency::event::irq, irq, vector,
                                           level);
        }
        if (irq == sys::platform::interrupt::virtual_timer_irq) {
            if (level != 2U || !sys::arch::hypervisor::handle_guest_virtual_timer_irq()) {
                const sys::kernel::interrupt::timing::latency_scope preemption_latency{
                    sys::kernel::interrupt::timing::latency_kind::preemption_service};
                const sys::u64 ticks = sys::platform::timer::handle_interrupt();
                {
                    const sys::kernel::object::read_guard object_read_guard{};
                    sys::kernel::hypervisor::poll_virtual_timers(sys::arch::cpu::current_id(),
                                                                  sys::arch::timer::counter());
                }
                if (sys::kernel::thread::user_execution_active[sys::arch::cpu::current_id()]) {
                    if (vector == 9U) {
                        sys::kernel::thread::schedule_user(*frame);
                    } else if (vector == 5U) {
                        (void)sys::kernel::thread::resume_user_from_idle(*frame);
                    }
                } else {
                    sys::kernel::scheduler::on_timer_tick();
                }
                /*
                 * One CPU only: the sweep is idempotent, but re-arming a
                 * line is a GIC write and there is nothing to gain from
                 * four CPUs racing to do the same one. See
                 * interrupt::recover_stormed() for why this has to run off
                 * the timer rather than out of the interrupt path.
                 */
                if (sys::arch::cpu::current_id() == 0U) {
                    sys::kernel::interrupt::recover_stormed(
                        sys::platform::timer::ticks(sys::arch::cpu::current_id()));
                    /*
                     * Same reasoning as the sweep above, and the same CPU:
                     * the drain reads every CPU's ring, so running it on four
                     * of them would just be four CPUs contending for one
                     * console lock to print the same records (OBS-003).
                     */
                    sys::printk::drain_deferred();
                }
                if (ticks == 1U && sys::arch::cpu::current_id() == 0U) {
                    sys::printk::defer(sys::kernel::emergency::event::irq,
                                                static_cast<sys::u64>(sys::arch::cpu::current_id()),
                                                ticks);
                }
            }
        } else if (irq == sys::platform::interrupt::virtual_gic_maintenance_irq) {
            (void)sys::arch::hypervisor::virtual_gic_maintenance_status();
        } else if (irq == sys::platform::interrupt::reschedule_ipi) {
            sys::kernel::interrupt::timing::complete_cross_cpu_wake(sys::arch::cpu::current_id());
            sys::arch::smp::record_reschedule_ipi();
#if CONFIG_HYPERVISOR_SELFTEST
            {
                const sys::kernel::object::read_guard object_read_guard{};
                sys::kernel::hypervisor::test::service_real_smp_job(sys::arch::cpu::current_id());
            }
#endif
#if CONFIG_TESTS
            // TST-019: the map/unmap side of the concurrent revocation race.
            // Same work lane as the SMP job above, and inert unless that test
            // has armed it.
            sys::kernel::tests::revoke_race::service_job(sys::arch::cpu::current_id());
            sys::kernel::tests::lifecycle_race::service_job(sys::arch::cpu::current_id());
            sys::kernel::tests::allocator_race::service_job(sys::arch::cpu::current_id());
#endif
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
        } else {
            const sys::kernel::object::read_guard object_read_guard{};
            const auto dispatched = sys::kernel::interrupt::dispatch(irq);
            userspace_deactivate = dispatched.delivered;
            // Finishing the signal here, not inside dispatch(), is what
            // keeps interrupt.hh from needing scheduler.hh -- see
            // dispatch_result's own comment. Still inside the read guard:
            // resolving bound_thread inside signal_notification() needs
            // the same protection dispatch() itself already relied on to
            // resolve the interrupt's own notification reference.
            if (dispatched.target != nullptr)
                sys::kernel::thread::signal_notification(*dispatched.target, dispatched.badge);
        }
        if ((irq == sys::platform::interrupt::virtual_timer_irq && level != 2U) ||
            irq == sys::platform::interrupt::reschedule_ipi) {
            const sys::cpu_id_t cpu = sys::arch::cpu::current_id();
            const sys::u64 now = sys::platform::timer::ticks(cpu);
            const sys::u64 thread_deadline = sys::kernel::thread::next_timer_deadline(cpu, now);
            sys::platform::timer::program_deadline(
                cpu, sys::kernel::hypervisor::next_virtual_timer_deadline(cpu, thread_deadline));
        }
        if (irq < 1020U) {
            sys::platform::interrupt::complete(irq);
            if (!userspace_deactivate)
                sys::platform::interrupt::deactivate(irq);
        }
        return;
    }

    if (exception_class == 0U) {
        const sys::kernel::object::read_guard object_read_guard{};
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

    const sys::u64 fault_address =
        sys::arch::exception::fault_address(static_cast<sys::u32>(level));
    sys::kernel::panic::stop(sys::kernel::panic::reason::fatal_exception,
                             static_cast<sys::u32>(level), vector, syndrome, fault_address,
                             frame->instruction_pointer);
}
