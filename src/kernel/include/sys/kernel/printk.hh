#pragma once
#include <sys/arch/cpu.hh>
#include <sys/arch/irq.hh>
#include <sys/arch/timer.hh>
#include <sys/kernel/emergency.hh>
#include <sys/kernel/interrupt/timing.hh>
#include <sys/platform/platform.hh>
#include <sys/types.hh>

#include <stdarg.h>

namespace sys::printk
{
    inline volatile u32 raw_lock{};
    inline u64 timestamp_baseline{};
    inline bool timestamp_initialized{};

    [[nodiscard]] inline bool lock() noexcept {
        constexpr u32 maximum_attempts = 4096U;
        for (u32 attempt = 0U; attempt < maximum_attempts; ++attempt) {
            u32 expected = 0U;
            if (__atomic_compare_exchange_n(&raw_lock, &expected, 1U, false, __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED))
                return true;
            arch::cpu::relax();
        }
        kernel::emergency::append(kernel::emergency::event::printk_contention);
        return false;
    }

    inline void unlock() noexcept {
        __atomic_store_n(&raw_lock, 0U, __ATOMIC_RELEASE);
    }

    enum class length_t : u8 {
        none,
        l,
        ll,
        z,
    };

    // RT and exception paths enqueue structured records without taking the
    // console lock or changing interrupt state.
    inline void defer(kernel::emergency::event kind, u64 argument0 = 0U, u64 argument1 = 0U,
                      u64 argument2 = 0U, u64 argument3 = 0U, u64 argument4 = 0U) noexcept {
        kernel::emergency::append(kind, argument0, argument1, argument2, argument3, argument4);
    }

    inline void putc(char value) noexcept {
        platform::console::putc(value);
    }

    inline usize_t puts(const char* text) noexcept {
        if (text == nullptr) {
            text = "(null)";
        }

        usize_t count = 0U;
        while (*text != '\0') {
            putc(*text++);
            ++count;
        }
        return count;
    }

    inline usize_t put_unsigned(u64 value, u32 base, bool uppercase = false) noexcept {
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        char buffer[sizeof(u64) * bits_per_byte];
        usize_t length = 0U;

        do {
            const auto digit = static_cast<usize_t>(value % static_cast<u64>(base));
            buffer[length++] = digits[digit];
            value /= static_cast<u64>(base);
        } while (value != 0U);

        const usize_t count = length;
        while (length != 0U) {
            putc(buffer[--length]);
        }
        return count;
    }

    inline usize_t put_signed(s64 value) noexcept {
        if (value < 0) {
            putc('-');
            return 1U + put_unsigned(static_cast<u64>(-(value + 1)) + 1U, 10U);
        }
        return put_unsigned(static_cast<u64>(value), 10U);
    }

    inline void put_timestamp() noexcept {
#if CONFIG_PRINTK_TIME
        const u64 frequency = arch::timer::frequency();
        u64 seconds = 0U;
        u64 microseconds = 0U;
        if (frequency != 0U) {
            const u64 now = arch::timer::counter();
            if (!timestamp_initialized) {
                timestamp_baseline = now;
                timestamp_initialized = true;
            }
            const u64 elapsed = now - timestamp_baseline;
            seconds = elapsed / frequency;
            microseconds = (elapsed % frequency) * 1000000U / frequency;
        }
        putc('[');
        u32 digits = 1U;
        for (u64 value = seconds; value >= 10U; value /= 10U)
            ++digits;
        for (u32 width = digits; width < 5U; ++width)
            putc(' ');
        put_unsigned(seconds, 10U);
        putc('.');
        const u64 divisors[] = {100000U, 10000U, 1000U, 100U, 10U, 1U};
        for (const u64 divisor : divisors)
            putc(static_cast<char>('0' + microseconds / divisor % 10U));
        puts("] ");
#endif
    }

    inline int vprintk(const char* format, va_list arguments) noexcept {
        if (format == nullptr) {
            return static_cast<int>(puts("(null)"));
        }

        usize_t count = 0U;

        while (*format != '\0') {
            if (*format != '%') {
                putc(*format++);
                ++count;
                continue;
            }

            ++format;
            if (*format == '%') {
                putc('%');
                ++format;
                ++count;
                continue;
            }

            length_t length = length_t::none;
            if (format[0] == 'l' && format[1] == 'l') {
                length = length_t::ll;
                format += 2;
            } else if (format[0] == 'l') {
                length = length_t::l;
                ++format;
            } else if (format[0] == 'z') {
                length = length_t::z;
                ++format;
            }

            switch (*format) {
                case 'c':
                    putc(static_cast<char>(va_arg(arguments, int)));
                    ++count;
                    break;

                case 's':
                    count += puts(va_arg(arguments, const char*));
                    break;

                case 'd':
                case 'i': {
                    s64 value;
                    switch (length) {
                        case length_t::ll:
                            value = static_cast<s64>(va_arg(arguments, long long));
                            break;
                        case length_t::l:
                            value = static_cast<s64>(va_arg(arguments, long));
                            break;
                        case length_t::z:
                            value = static_cast<s64>(va_arg(arguments, isize_t));
                            break;
                        case length_t::none:
                        default:
                            value = static_cast<s64>(va_arg(arguments, int));
                            break;
                    }
                    count += put_signed(value);
                    break;
                }

                case 'u':
                case 'x':
                case 'X': {
                    u64 value;
                    switch (length) {
                        case length_t::ll:
                            value = static_cast<u64>(va_arg(arguments, unsigned long long));
                            break;
                        case length_t::l:
                            value = static_cast<u64>(va_arg(arguments, unsigned long));
                            break;
                        case length_t::z:
                            value = static_cast<u64>(va_arg(arguments, usize_t));
                            break;
                        case length_t::none:
                        default:
                            value = static_cast<u64>(va_arg(arguments, unsigned int));
                            break;
                    }

                    const bool hexadecimal = *format == 'x' || *format == 'X';
                    const bool uppercase = *format == 'X';
                    count += put_unsigned(value, hexadecimal ? 16U : 10U, uppercase);
                    break;
                }

                case 'p': {
                    const auto pointer = va_arg(arguments, const void*);
                    count += puts("0x");
                    count +=
                        put_unsigned(static_cast<u64>(reinterpret_cast<uintptr_t>(pointer)), 16U);
                    break;
                }

                case '\0':
                    putc('%');
                    ++count;
                    return static_cast<int>(count);

                default:
                    putc('%');
                    putc(*format);
                    count += 2U;
                    break;
            }

            if (*format != '\0') {
                ++format;
            }
        }

        return static_cast<int>(count);
    }

    inline int printk(const char* format, ...) noexcept {
        const kernel::interrupt::timing::state irq_state =
            kernel::interrupt::timing::save_and_disable();
        if (!lock()) {
            kernel::interrupt::timing::restore(irq_state);
            return -1;
        }

        // Keep the timing sample on the lock path only.
        kernel::interrupt::timing::restore(irq_state);

        put_timestamp();

        va_list arguments;
        va_start(arguments, format);
        const int result = vprintk(format, arguments);
        va_end(arguments);

        unlock();
        return result;
    }

    /*
     * Asynchronous draining of deferred records (OBS-003).
     *
     * printk() drops the message entirely when the console lock stays
     * contended for 4096 attempts, leaving only a bare printk_contention
     * marker in the ring. That was the whole of the deferral: the record was
     * written and nothing ever read it back, so every dropped line was
     * invisible until someone attached a debugger and walked the buffers by
     * hand, and the ring wrapped, so the evidence expired on its own too.
     *
     * This formats `emergency::deferred` -- the dedicated reportable-record
     * ring, not the per-CPU trace buffers. See emergency.hh for why the
     * trace buffers cannot serve: under CONFIG_TRACE they wrap hundreds of
     * times a second and a printk_contention record never survives long
     * enough to be printed.
     */
    [[nodiscard]] inline const char* event_name(kernel::emergency::event kind) noexcept {
        switch (kind) {
            case kernel::emergency::event::printk_contention:
                return "printk-contention";
            case kernel::emergency::event::fatal_exception:
                return "fatal-exception";
            case kernel::emergency::event::stack_corruption:
                return "stack-corruption";
            case kernel::emergency::event::user_fault:
                return "user-fault";
            case kernel::emergency::event::device_assign:
                return "device-assign";
            case kernel::emergency::event::device_revoke:
                return "device-revoke";
            default:
                return "other";
        }
    }

    // How far the deferred ring has been formatted. Only the drain writes
    // these, and the drain runs on one CPU, so they need no atomics.
    inline u64 drained{};
    inline u64 drain_lost{};
    inline u64 drain_lost_reported{};
    inline volatile u32 drain_active{};

    inline void drain_deferred() noexcept {
        /*
         * Bounded per call: this runs off the timer interrupt, and a fault
         * storm must not turn one tick into an unbounded console write.
         * Whatever is left stays in the ring for the next tick.
         */
        constexpr u32 budget = 4U;

        /*
         * Never even attempt the console while it is busy.
         *
         * This runs off the timer interrupt, so it can interrupt the console
         * lock holder ON ITS OWN CPU -- and then no amount of spinning can
         * win, because the holder cannot run to release it until this handler
         * returns. printk()'s 4096-attempt bailout keeps that from being a
         * hang, but the cost is 4096 wasted spins inside an interrupt for a
         * line that is dropped anyway. Measured first: with an unconditional
         * printk here the drain was called and produced no output at all.
         *
         * Reading the lock word first turns that into a cheap skip. If it is
         * free, this CPU is not the holder and the CAS below will take it
         * uncontended; if another CPU grabs it in between, printk returns -1
         * and the record waits for the next tick rather than being lost.
         */
        if (__atomic_load_n(&raw_lock, __ATOMIC_ACQUIRE) != 0U)
            return;

        // Re-entrancy guard. The drain calls printk, which on failure appends
        // its own contention record; without this a nested drain could chase
        // records it is itself producing.
        u32 expected = 0U;
        if (!__atomic_compare_exchange_n(&drain_active, &expected, 1U, false, __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED))
            return;

        const u64 produced =
            __atomic_load_n(&kernel::emergency::deferred_sequence, __ATOMIC_ACQUIRE);
        u64 cursor = drained;

        /*
         * Records older than the ring depth are gone. Say how many rather
         * than silently resuming at the oldest survivor: a gap in the record
         * stream is itself a finding, and one that only appears under exactly
         * the load that makes the records matter.
         */
        if (produced - cursor > kernel::emergency::deferred_capacity) {
            drain_lost += produced - cursor - kernel::emergency::deferred_capacity;
            cursor = produced - kernel::emergency::deferred_capacity;
        }

        const u64 limit = (produced - cursor > budget) ? cursor + budget : produced;
        while (cursor < limit) {
            const u64 wanted = cursor + 1U;
            const kernel::emergency::record& slot =
                kernel::emergency::deferred[wanted % kernel::emergency::deferred_capacity];

            if (__atomic_load_n(&slot.sequence, __ATOMIC_ACQUIRE) != wanted) {
                // Reserved by a concurrent append but not yet published.
                break;
            }

            const kernel::emergency::event kind = slot.kind;
            const auto cpu = static_cast<unsigned>(slot.cpu);
            const u64 argument0 = slot.argument[0];
            const u64 argument1 = slot.argument[1];
            const u64 argument2 = slot.argument[2];
            const u64 argument3 = slot.argument[3];

            /*
             * Re-read the sequence after copying. A wrap that landed on this
             * slot mid-copy would otherwise be reported as a real record with
             * fields from two different events spliced together, which is
             * worse than reporting nothing.
             */
            if (__atomic_load_n(&slot.sequence, __ATOMIC_ACQUIRE) != wanted) {
                ++drain_lost;
                ++cursor;
                continue;
            }

            const int written =
                printk("[DEFER] cpu=%u seq=%llu kind=%s a0=0x%llx a1=0x%llx a2=0x%llx "
                       "a3=0x%llx\n",
                       cpu, static_cast<unsigned long long>(wanted), event_name(kind),
                       static_cast<unsigned long long>(argument0),
                       static_cast<unsigned long long>(argument1),
                       static_cast<unsigned long long>(argument2),
                       static_cast<unsigned long long>(argument3));

            /*
             * The console was busy. Leave the cursor where it is so the
             * record is retried rather than dropped -- dropping it here would
             * reproduce the exact bug this drain exists to fix.
             */
            if (written < 0)
                break;

            ++cursor;
        }

        drained = cursor;

        /*
         * Report the gap once per change rather than per lost record: the
         * condition that loses records is the one where console bandwidth is
         * already the bottleneck, so a line per loss would deepen the hole it
         * is reporting.
         */
        if (drain_lost != drain_lost_reported) {
            if (printk("[DEFER] lost=%llu\n", static_cast<unsigned long long>(drain_lost)) >= 0)
                drain_lost_reported = drain_lost;
        }

        __atomic_store_n(&drain_active, 0U, __ATOMIC_RELEASE);
    }
} // namespace sys::printk

#define printk(...) ::sys::printk::printk(__VA_ARGS__)
#define pr_err(format, ...) printk("[ERR] " format __VA_OPT__(, ) __VA_ARGS__)
#define pr_warn(format, ...) printk("[WARN] " format __VA_OPT__(, ) __VA_ARGS__)
#define pr_info(format, ...) printk("[INFO] " format __VA_OPT__(, ) __VA_ARGS__)
#if defined(CONFIG_DEBUG)
#define pr_debug(format, ...) printk("[DEBUG] " format __VA_OPT__(, ) __VA_ARGS__)
#else
#define pr_debug(...)                                                                              \
    do {                                                                                           \
    } while (false)
#endif
