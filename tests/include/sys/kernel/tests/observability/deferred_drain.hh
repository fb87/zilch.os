#pragma once

#include <sys/kernel/emergency.hh>
#include <sys/kernel/printk.hh>

namespace sys::kernel::tests::deferred_drain
{
    /*
     * Deferred records are formatted asynchronously (OBS-003).
     *
     * printk() drops a message outright once the console lock has stayed
     * contended for 4096 attempts, leaving a bare printk_contention record
     * behind. Nothing ever read those back, so a dropped console line was
     * unrecoverable in practice -- and the ring wrapped, so it did not even
     * stay unrecoverable for long.
     *
     * Two things are checked here, both of which were wrong in the first
     * implementation and neither of which a live boot demonstrates:
     *
     *  1. The reportable/trace partition. Draining the per-CPU trace rings
     *     was tried first and could never work: under CONFIG_TRACE they carry
     *     every IRQ and IPC and wrap hundreds of times a second, so a
     *     contention record was always overwritten before it could be
     *     printed. Reportable kinds are now mirrored into a dedicated ring,
     *     and a trace kind must stay out of it -- if trace traffic leaked in,
     *     the dedicated ring would wrap just as fast and the fix would
     *     quietly regress to the broken version.
     *
     *  2. That the drain actually consumes what it is given. The cursor is
     *     advanced by the same loop that prints, and a bug that advanced it
     *     without printing would look identical from outside: a silent
     *     console either way.
     */
    [[nodiscard]] inline error_t run() noexcept
    {
        constexpr u64 trace_magic = 0x7face000ULL;
        constexpr u64 report_magic = 0x4ecc0de0ULL;

        const auto present = [](u64 magic) noexcept {
            for (u32 index = 0U; index < emergency::deferred_capacity; ++index) {
                const emergency::record& entry = emergency::deferred[index];
                if (__atomic_load_n(&entry.sequence, __ATOMIC_ACQUIRE) != 0U &&
                    entry.argument[0] == magic)
                    return true;
            }
            return false;
        };

        /*
         * Searching the ring by magic rather than asserting an exact
         * sequence delta: other CPUs are live and may append a genuine
         * reportable record at any moment, and a test that failed because a
         * real fault was recorded alongside it would be worse than no test.
         */
        emergency::append(emergency::event::irq, trace_magic);
        if (present(trace_magic))
            return error_t::invalid_argument; // a trace kind reached the deferred ring

        emergency::append(emergency::event::device_assign, report_magic);
        if (!present(report_magic))
            return error_t::invalid_argument; // a reportable kind did not

        // Find the sequence the mirrored record was published under.
        u64 target = 0U;
        for (u32 index = 0U; index < emergency::deferred_capacity; ++index) {
            const emergency::record& entry = emergency::deferred[index];
            const u64 sequence = __atomic_load_n(&entry.sequence, __ATOMIC_ACQUIRE);
            if (sequence != 0U && entry.argument[0] == report_magic && sequence > target)
                target = sequence;
        }
        if (target == 0U)
            return error_t::invalid_argument;

        /*
         * Drive the drain directly rather than waiting for the timer. The
         * drain is deliberately bounded per call, so it takes several to
         * clear a backlog; the bound on the loop is what keeps this test from
         * hanging if the cursor ever stops advancing, which is exactly the
         * failure worth catching.
         */
        constexpr u32 maximum_calls = 256U;
        u32 calls = 0U;
        while (printk::drained < target && calls < maximum_calls) {
            printk::drain_deferred();
            ++calls;
        }
        if (printk::drained < target)
            return error_t::invalid_argument; // drain never consumed the record

        pr_info("[TEST] name=deferred_record_drain result=PASS trace_excluded=1 "
                "reportable_mirrored=1 drained_to=%llu calls=%u lost=%llu\n",
                static_cast<unsigned long long>(printk::drained), calls,
                static_cast<unsigned long long>(printk::drain_lost));
        return error_t::success;
    }
} // namespace sys::kernel::tests::deferred_drain
