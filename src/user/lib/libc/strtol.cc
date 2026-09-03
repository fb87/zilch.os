#include <stdlib.h>

namespace
{
    [[nodiscard]] bool space(char value) noexcept {
        return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\v' ||
               value == '\f';
    }

    [[nodiscard]] int digit_value(char value) noexcept {
        if (value >= '0' && value <= '9')
            return value - '0';
        if (value >= 'a' && value <= 'z')
            return value - 'a' + 10;
        if (value >= 'A' && value <= 'Z')
            return value - 'A' + 10;
        return -1;
    }
} // namespace

extern "C" {

long strtol(const char* text, char** end, int base) noexcept {
    const char* cursor = text;
    while (space(*cursor))
        ++cursor;

    bool negative = false;
    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        ++cursor;
    }

    if (base == 0) {
        if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
            base = 16;
            cursor += 2;
        } else if (cursor[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16 && cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        cursor += 2;
    }

    /*
     * Accumulated unsigned so the most negative long is representable on the
     * way in; a signed accumulator would overflow one step before the value
     * it is meant to produce.
     */
    unsigned long magnitude = 0U;
    bool any = false;
    for (;; ++cursor) {
        const int digit = digit_value(*cursor);
        if (digit < 0 || digit >= base)
            break;
        magnitude = magnitude * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
        any = true;
    }

    if (end != nullptr)
        *end = const_cast<char*>(any ? cursor : text);
    if (!any)
        return 0;
    return negative ? -static_cast<long>(magnitude) : static_cast<long>(magnitude);
}

int atoi(const char* text) noexcept {
    return static_cast<int>(strtol(text, nullptr, 10));
}

} // extern "C"
