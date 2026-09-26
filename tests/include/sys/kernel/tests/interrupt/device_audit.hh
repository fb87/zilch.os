#pragma once

#include <sys/kernel/emergency.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::device_audit
{
    /*
     * Device assignment leaves an audit trail (OBS-008).
     *
     * Worth an actual test rather than a claim, because these records are
     * write-only from the system's point of view: the emergency ring is
     * post-mortem storage that nothing formats to a live console, so an
     * append that silently never happened would look exactly like one that
     * did. Scanning the ring is the only way to know the trail exists.
     *
     * By the time the bootstrap self-test runs, root has already been given
     * the serial UART's MMIO frame and its interrupt line, so at least one
     * assignment record must be present. Finding none means either the
     * emit sites are unreachable or the events are being recorded under a
     * kind nobody will look for.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        u32 assigns = 0U;
        u32 revokes = 0U;
        u32 malformed = 0U;
        u32 faults = 0U;

        for (u32 cpu = 0U; cpu < emergency::cpu_count; ++cpu) {
            for (u32 slot = 0U; slot < emergency::records_per_cpu; ++slot) {
                const emergency::record& entry = emergency::buffers[cpu][slot];
                if (__atomic_load_n(&entry.sequence, __ATOMIC_ACQUIRE) == 0U)
                    continue; // never written
                if (entry.kind == emergency::event::user_fault) {
                    /*
                     * USR-037: a fault record must survive into release, and
                     * must carry the syndrome -- "a fault happened" without
                     * an ESR cannot distinguish an undefined instruction
                     * from a translation fault, which was the difference
                     * between two separate defects while chasing the boot
                     * stall (checklist 0141).
                     */
                    ++faults;
                    if (entry.argument[1] == 0U)
                        ++malformed;
                    continue;
                }
                if (entry.kind != emergency::event::device_assign &&
                    entry.kind != emergency::event::device_revoke)
                    continue;

                /*
                 * A record naming no device is worse than no record: it
                 * says an assignment happened and refuses to say of what.
                 * Both emit sites pass an IRQ number or a physical address,
                 * and neither is legitimately zero here.
                 */
                if (entry.version != emergency::record_format_version || entry.argument[0] == 0U)
                    ++malformed;
                if (entry.kind == emergency::event::device_assign)
                    ++assigns;
                else
                    ++revokes;
            }
        }

        if (malformed != 0U)
            return error_t::invalid_argument;
        if (assigns == 0U)
            return error_t::invalid_argument; // the UART frame and line alone should produce these

        pr_info("[TEST] name=device_assignment_audit result=PASS assigns=%u revokes=%u "
                "faults=%u malformed=0\n",
                assigns, revokes, faults);
        return error_t::success;
    }
} // namespace sys::kernel::tests::device_audit
