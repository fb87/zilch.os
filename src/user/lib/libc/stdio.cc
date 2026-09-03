#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Formatting is built on one bounded writer: everything renders into a
 * caller-supplied buffer, and printf is that plus a write(). Rendering
 * straight to the console instead would make snprintf the odd one out and
 * would turn every conversion into a stream of one-byte IPC calls.
 */
namespace
{
    struct sink {
        char* buffer;
        size_t capacity;
        size_t written; // counts what WOULD be written, per snprintf's contract
    };

    void emit(sink& out, char value) noexcept {
        if (out.buffer != nullptr && out.written + 1U < out.capacity)
            out.buffer[out.written] = value;
        ++out.written;
    }

    void emit_text(sink& out, const char* text) noexcept {
        for (const char* cursor = text; *cursor != '\0'; ++cursor)
            emit(out, *cursor);
    }

    void emit_padded(sink& out, const char* text, size_t width, bool zero_pad) noexcept {
        const size_t length = strlen(text);
        for (size_t index = length; index < width; ++index)
            emit(out, zero_pad ? '0' : ' ');
        emit_text(out, text);
    }

    /* Renders into `store` backwards, which avoids needing to know the digit
     * count up front, and returns a pointer into it. */
    char* render_unsigned(unsigned long value, unsigned base, bool uppercase,
                          char (&store)[24]) noexcept {
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        char* cursor = &store[23];
        *cursor = '\0';
        do {
            --cursor;
            *cursor = digits[value % base];
            value /= base;
        } while (value != 0U);
        return cursor;
    }

    char* render_signed(long value, char (&store)[24]) noexcept {
        const bool negative = value < 0;
        /* Negate in unsigned space so the most negative long does not
         * overflow on the way to being printed. */
        const unsigned long magnitude =
            negative ? 0U - static_cast<unsigned long>(value) : static_cast<unsigned long>(value);
        char* cursor = render_unsigned(magnitude, 10U, false, store);
        if (negative) {
            --cursor;
            *cursor = '-';
        }
        return cursor;
    }

    int format_into(sink& out, const char* format, va_list arguments) noexcept {
        char store[24];
        for (const char* cursor = format; *cursor != '\0'; ++cursor) {
            if (*cursor != '%') {
                emit(out, *cursor);
                continue;
            }
            ++cursor;
            if (*cursor == '\0')
                break;

            bool zero_pad = false;
            if (*cursor == '0') {
                zero_pad = true;
                ++cursor;
            }
            size_t width = 0U;
            while (*cursor >= '0' && *cursor <= '9') {
                width = width * 10U + static_cast<size_t>(*cursor - '0');
                ++cursor;
            }
            /*
             * The length modifier decides the va_arg type, and getting it
             * wrong is not cosmetic. AAPCS64 leaves the upper bits of a
             * promoted `int` argument unspecified, so reading one as `long`
             * yields whatever happened to be in the register above the
             * value -- which showed up immediately as a mangled %d.
             */
            bool is_long = false;
            while (*cursor == 'l' || *cursor == 'z' || *cursor == 'h') {
                if (*cursor != 'h')
                    is_long = true;
                ++cursor;
            }

            switch (*cursor) {
                case 's': {
                    const char* text = va_arg(arguments, const char*);
                    emit_padded(out, text != nullptr ? text : "(null)", width, false);
                    break;
                }
                case 'c':
                    emit(out, static_cast<char>(va_arg(arguments, int)));
                    break;
                case 'd':
                case 'i': {
                    const long value =
                        is_long ? va_arg(arguments, long) : static_cast<long>(va_arg(arguments, int));
                    emit_padded(out, render_signed(value, store), width, zero_pad);
                    break;
                }
                case 'u':
                case 'x':
                case 'X': {
                    const unsigned long value =
                        is_long ? va_arg(arguments, unsigned long)
                                : static_cast<unsigned long>(va_arg(arguments, unsigned int));
                    const unsigned base = *cursor == 'u' ? 10U : 16U;
                    emit_padded(out, render_unsigned(value, base, *cursor == 'X', store), width,
                                zero_pad);
                    break;
                }
                case 'p': {
                    emit_text(out, "0x");
                    const auto value = reinterpret_cast<unsigned long>(va_arg(arguments, void*));
                    emit_padded(out, render_unsigned(value, 16U, false, store), width, zero_pad);
                    break;
                }
                case '%':
                    emit(out, '%');
                    break;
                default:
                    /* Unrecognised: show it rather than swallow it. */
                    emit(out, '%');
                    emit(out, *cursor);
                    break;
            }
        }
        if (out.buffer != nullptr && out.capacity != 0U) {
            const size_t terminator = out.written < out.capacity ? out.written : out.capacity - 1U;
            out.buffer[terminator] = '\0';
        }
        return static_cast<int>(out.written);
    }

    /* One shared staging buffer: printf is not reentrant here, and this
     * userspace has no signals and no threads sharing a heap. */
    char print_buffer[512];
} // namespace

extern "C" {

int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments) noexcept {
    sink out{buffer, size, 0U};
    return format_into(out, format, arguments);
}

int snprintf(char* buffer, size_t size, const char* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return written;
}

int dprintf(int descriptor, const char* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(print_buffer, sizeof(print_buffer), format, arguments);
    va_end(arguments);
    if (written < 0)
        return written;
    const size_t length = strnlen(print_buffer, sizeof(print_buffer));
    return static_cast<int>(write(descriptor, print_buffer, length));
}

int printf(const char* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(print_buffer, sizeof(print_buffer), format, arguments);
    va_end(arguments);
    if (written < 0)
        return written;
    const size_t length = strnlen(print_buffer, sizeof(print_buffer));
    return static_cast<int>(write(STDOUT_FILENO, print_buffer, length));
}

int putchar(int character) noexcept {
    const char value = static_cast<char>(character);
    return write(STDOUT_FILENO, &value, 1U) == 1 ? character : -1;
}

int puts(const char* text) noexcept {
    const size_t length = strlen(text);
    if (write(STDOUT_FILENO, text, length) < 0)
        return -1;
    return putchar('\n') < 0 ? -1 : 0;
}

char* fgets(char* buffer, int size, void*) noexcept {
    if (buffer == nullptr || size <= 1)
        return nullptr;
    int index = 0;
    while (index < size - 1) {
        char value = '\0';
        if (read(STDIN_FILENO, &value, 1U) != 1)
            break;
        buffer[index++] = value;
        if (value == '\n')
            break;
    }
    buffer[index] = '\0';
    return index == 0 ? nullptr : buffer;
}

} // extern "C"
