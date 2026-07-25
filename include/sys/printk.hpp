#pragma once

#include <sys/platform/current.hpp>
#include <sys/types.hpp>

#include <stdarg.h>

namespace sys::printk
{
    namespace detail
    {
        inline constexpr char null_text[] = "(null)";

        inline void write_character(char value) noexcept {
            platform::current::platform_ops().console.putc(value);
        }

        inline void write_string(const char* text) noexcept {
            if (text == nullptr) {
                text = null_text;
            }

            while (*text != '\0') {
                write_character(*text);
                ++text;
            }
        }

        inline void write_unsigned(u64 value, u32 base) noexcept {
            constexpr char digits[] = "0123456789abcdef";

            char buffer[sizeof(u64) * 8U];
            usize_t length = 0U;

            if (base < 2U || base > 16U) {
                return;
            }

            do {
                const u64 digit = value % static_cast<u64>(base);

                buffer[length] = digits[digit];
                ++length;

                value /= static_cast<u64>(base);
            } while (value != 0U);

            while (length != 0U) {
                --length;
                write_character(buffer[length]);
            }
        }

        inline void write_signed(s64 value) noexcept {
            if (value < 0) {
                write_character('-');

                /*
                 * Avoid overflowing when value is the minimum signed value.
                 *
                 * For example:
                 *
                 *   magnitude(-INT64_MIN)
                 *
                 * cannot be calculated by simply using -value.
                 */
                const u64 magnitude = static_cast<u64>(-(value + 1)) + 1U;

                write_unsigned(magnitude, 10U);
                return;
            }

            write_unsigned(static_cast<u64>(value), 10U);
        }

        inline void write_pointer(const void* pointer) noexcept {
            write_string("0x");

            write_unsigned(static_cast<u64>(reinterpret_cast<uintptr_t>(pointer)), 16U);
        }

        inline void write_unknown_format(char specifier) noexcept {
            write_character('%');

            if (specifier != '\0') {
                write_character(specifier);
            }
        }

        inline void vprint(const char* format, va_list arguments) noexcept {
            if (format == nullptr) {
                write_string(null_text);
                return;
            }

            while (*format != '\0') {
                if (*format != '%') {
                    write_character(*format);
                    ++format;
                    continue;
                }

                ++format;

                if (*format == '\0') {
                    write_character('%');
                    break;
                }

                bool long_long = false;

                if (format[0] == 'l' && format[1] == 'l') {
                    long_long = true;
                    format += 2;
                }

                switch (*format) {
                    case '%':
                        write_character('%');
                        break;

                    case 'c':
                        write_character(static_cast<char>(va_arg(arguments, int)));
                        break;

                    case 's':
                        write_string(va_arg(arguments, const char*));
                        break;

                    case 'd':
                    case 'i':
                        if (long_long) {
                            write_signed(va_arg(arguments, long long));
                        }
                        else {
                            write_signed(static_cast<s64>(va_arg(arguments, int)));
                        }
                        break;

                    case 'u':
                        if (long_long) {
                            write_unsigned(va_arg(arguments, unsigned long long), 10U);
                        }
                        else {
                            write_unsigned(static_cast<u64>(va_arg(arguments, unsigned int)), 10U);
                        }
                        break;

                    case 'x':
                        if (long_long) {
                            write_unsigned(va_arg(arguments, unsigned long long), 16U);
                        }
                        else {
                            write_unsigned(static_cast<u64>(va_arg(arguments, unsigned int)), 16U);
                        }
                        break;

                    case 'p':
                        write_pointer(va_arg(arguments, const void*));
                        break;

                    default:
                        write_unknown_format(*format);
                        break;
                }

                ++format;
            }
        }
    } // namespace detail

    inline void putc(char value) noexcept {
        detail::write_character(value);
    }

    inline void puts(const char* text) noexcept {
        detail::write_string(text);
    }

#if defined(__clang__) || defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    inline void print(const char* format, ...) noexcept {
        va_list arguments;

        va_start(arguments, format);
        detail::vprint(format, arguments);
        va_end(arguments);
    }
} // namespace sys::printk

#define printk(...)           		::sys::printk::print(__VA_ARGS__)
#define pr_emerg(format, ...) 		printk("[EMERG] " format, ##__VA_ARGS__)
#define pr_alert(format, ...) 		printk("[ALERT] " format, ##__VA_ARGS__)
#define pr_crit(format, ...)  		printk("[CRIT] " format, ##__VA_ARGS__)
#define pr_err(format, ...)   		printk("[ERR] " format, ##__VA_ARGS__)
#define pr_warn(format, ...)  		printk("[WARN] " format, ##__VA_ARGS__)
#define pr_notice(format, ...) 		printk("[NOTICE] " format, ##__VA_ARGS__)
#define pr_info(format, ...) 		printk("[INFO] " format, ##__VA_ARGS__)

#if defined(CONFIG_DEBUG)
    #define pr_debug(format, ...) 	printk("[DEBUG] " format, ##__VA_ARGS__)
#else
    #define pr_debug(...)               do { } while (false)
#endif
