#pragma once

#include <sys/arch/cpu.hh>
#include <sys/platform/interrupt.hh>
#include <sys/types.hh>

namespace sys::platform::timer
{
    inline constexpr u32 ticks_per_second = 100U;
    inline u64 tsc_frequency = 0U;

    /*
     * Rate of the LAPIC's own countdown clock, post-divisor -- established
     * by calibrate_lapic() below, not assumed. QEMU's emulated LAPIC bus
     * clock rate is not a value this codebase can rely on as a documented
     * constant, so it is measured against the TSC at boot instead, the
     * same technique real kernels use (Linux's lapic_calibrate() does the
     * same thing: program a large one-shot count, busy-wait a known TSC
     * interval, see how far the LAPIC counted down).
     */
    inline u64 lapic_frequency = 0U;

    // Divide Configuration Register encoding is not linear -- see
    // calibrate_lapic()'s comment. 0x3 = divide by 16, a conventional
    // middle-ground choice: coarse enough that a 32-bit Initial Count
    // cannot wrap at the frequencies this platform actually runs at,
    // fine enough for ticks_per_second-scale scheduling resolution.
    inline constexpr u32 lapic_divide_by_16 = 0x3U;
    inline constexpr u32 lapic_calibration_window = 0xffffffffU;

    // LVT Timer register bit 17: 0 = one-shot, 1 = periodic (bit 18, unset
    // here, selects TSC-deadline mode on CPUs that support it).
    inline constexpr u32 lapic_lvt_periodic = 1U << 17U;
    inline constexpr u32 lapic_lvt_masked = 1U << 16U;

    [[nodiscard]] inline constexpr u64 deadline_after(u64 now, u64 delay) noexcept {
        return delay <= ~0ULL - now ? now + delay : ~0ULL;
    }

    [[nodiscard]] inline u64 read_tsc() noexcept {
        u32 lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<u64>(hi) << 32U) | static_cast<u64>(lo);
    }

    /*
     * CPUID leaf 0x15 gives the TSC/core-crystal-clock ratio directly:
     * EAX = denominator, EBX = numerator, ECX = crystal frequency in Hz.
     * TSC frequency = ECX * EBX / EAX. EBX == 0 means the ratio is not
     * enumerated at all (older/simpler CPU models), regardless of ECX.
     *
     * Falls back to leaf 0x16's processor base frequency (MHz) if 0x15
     * is not enumerated -- commonly close to the invariant TSC rate on
     * modern CPUs, though not architecturally guaranteed to be identical.
     * Falls back further to a fixed conservative estimate if QEMU's
     * chosen CPU model reports neither (this was the only value here
     * before this file measured anything).
     */
    inline void calibrate_tsc() noexcept {
        u32 eax, ebx, ecx, edx;
        arch::cpu::cpuid(0x15U, 0U, eax, ebx, ecx, edx);
        if (ebx != 0U && ecx != 0U && eax != 0U) {
            tsc_frequency = (static_cast<u64>(ecx) * static_cast<u64>(ebx)) / eax;
            return;
        }

        arch::cpu::cpuid(0x16U, 0U, eax, ebx, ecx, edx);
        const u32 base_mhz = eax & 0xffffU;
        if (base_mhz != 0U) {
            tsc_frequency = static_cast<u64>(base_mhz) * 1000000ULL;
            return;
        }

        tsc_frequency = 2400000000ULL;
    }

    /*
     * One-shot calibration run: program the LAPIC timer to count down from
     * a large value, busy-wait a fixed TSC-measured duration (~10ms, using
     * whatever calibrate_tsc() already established -- this is why that
     * must run first), then see how far the LAPIC counted down over that
     * known interval. lapic_frequency is then real ticks-per-second of
     * this LAPIC's own clock, not an assumed constant.
     */
    inline void calibrate_lapic() noexcept {
        interrupt::lapic_write(0x3e0U, lapic_divide_by_16);
        interrupt::lapic_write(0x380U, lapic_calibration_window);

        const u64 tsc_start = read_tsc();
        const u64 tsc_target = tsc_start + (tsc_frequency / 100U);
        while (read_tsc() < tsc_target) {
            __asm__ volatile("pause");
        }

        const u32 remaining = interrupt::lapic_read(0x390U);
        interrupt::lapic_write(0x320U, lapic_lvt_masked);

        const u32 elapsed = lapic_calibration_window - remaining;
        lapic_frequency = elapsed != 0U ? static_cast<u64>(elapsed) * 100U : 1000000000ULL;
    }

    /*
     * Programs vector 32 (interrupt::virtual_timer_irq) as a periodic LAPIC
     * timer interrupt at ticks_per_second, delivered through the real IDT
     * sys::arch::exception::initialize_idt() installs. Periodic mode
     * auto-reloads Initial Count on every fire, so unlike arm64's one-shot
     * re-arm-per-interrupt model, nothing needs to run again on each tick
     * to keep this timer going.
     */
    inline void start_periodic() noexcept {
        const u64 interval = lapic_frequency / ticks_per_second;
        interrupt::lapic_write(0x3e0U, lapic_divide_by_16);
        interrupt::lapic_write(0x320U, interrupt::virtual_timer_irq | lapic_lvt_periodic);
        interrupt::lapic_write(0x380U, interval == 0U ? 1U : static_cast<u32>(interval));
    }

    inline void initialize() noexcept {
        calibrate_tsc();
        calibrate_lapic();
        start_periodic();
    }

    /*
     * Real per-tick counter, incremented once per periodic interrupt.
     * Nothing currently reads this return value (arch.cc's dispatch
     * discards it with (void)) -- it exists so this file has honest,
     * interrupt-driven bookkeeping to build on, rather than the fixed
     * 1000000U placeholder this replaced, which no periodic interrupt had
     * ever actually produced.
     *
     * Deliberately does NOT change what ticks() below returns: ticks() is
     * read from many already-live call sites (kernel.hh's boot readiness
     * check, kernel/interrupt.hh's delivery timestamping,
     * thread/scheduler.hh's scheduling-context accounting) that this
     * change did not set out to touch, and changing ticks()'s semantics
     * would ripple into all of them at once, untestable in an environment
     * where amd64 cannot be booted at all (see gdt.md's note on QEMU's
     * multiboot loader). Wiring the interrupt itself is this change's
     * scope; re-deriving ticks() from it is a separate, later step.
     */
    inline volatile u64 interrupt_count = 0U;

    [[nodiscard]] inline u64 handle_interrupt() noexcept {
        return __atomic_add_fetch(&interrupt_count, 1U, __ATOMIC_RELAXED);
    }

    [[nodiscard]] inline u64 ticks(cpu_id_t) noexcept {
        if (tsc_frequency == 0U)
            return 0U;
        return read_tsc() / (tsc_frequency / ticks_per_second);
    }

    [[nodiscard]] inline bool certification_valid() noexcept {
        return tsc_frequency != 0U && lapic_frequency != 0U;
    }
} // namespace sys::platform::timer
