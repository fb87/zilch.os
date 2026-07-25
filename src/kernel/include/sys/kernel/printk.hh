#pragma once

#include <stdarg.h>

#include <sys/arch/irq.hh>
#include <sys/arch/cpu.hh>
#include <sys/platform/platform.hh>
#include <sys/types.hh>

namespace sys::printk
{
    inline volatile u32 raw_lock{};

    inline void lock() noexcept
    {
        while (__atomic_exchange_n(&raw_lock, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&raw_lock, __ATOMIC_RELAXED) != 0U) {
                arch::cpu::relax();
            }
        }
    }

    inline void unlock() noexcept
    {
        __atomic_store_n(&raw_lock, 0U, __ATOMIC_RELEASE);
    }

    enum class length_t : u8 {
        none,
        l,
        ll,
        z,
    };

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
                count += put_unsigned(
                    static_cast<u64>(reinterpret_cast<uintptr_t>(pointer)), 16U);
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

    inline int printk(const char* format, ...) noexcept
    {
        const arch::irq::irq_state_t irq_state = arch::irq::save_and_disable();
        lock();

        va_list arguments;
        va_start(arguments, format);
        const int result = vprintk(format, arguments);
        va_end(arguments);

        unlock();
        arch::irq::restore(irq_state);
        return result;
    }
} // namespace sys::printk

#define printk(...) ::sys::printk::printk(__VA_ARGS__)
#define pr_err(format, ...) printk("[ERR] " format __VA_OPT__(,) __VA_ARGS__)
#define pr_warn(format, ...) printk("[WARN] " format __VA_OPT__(,) __VA_ARGS__)
#define pr_info(format, ...) printk("[INFO] " format __VA_OPT__(,) __VA_ARGS__)
#if defined(CONFIG_DEBUG)
#define pr_debug(format, ...) printk("[DEBUG] " format __VA_OPT__(,) __VA_ARGS__)
#else
#define pr_debug(...) do { } while (false)
#endif
