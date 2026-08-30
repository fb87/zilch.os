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
