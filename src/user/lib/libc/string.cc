#include <string.h>

/*
 * Straightforward byte-at-a-time implementations. They are deliberately not
 * word-optimised: nothing here is on a measured hot path, and the certified
 * latency bounds are dominated by IPC round trips, not by string handling.
 * Correctness under -Wconversion is worth more than cycles that nothing is
 * waiting on.
 */
extern "C" {

void* memcpy(void* destination, const void* source, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (size_t index = 0U; index < count; ++index)
        out[index] = in[index];
    return destination;
}

void* memmove(void* destination, const void* source, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    if (out == in || count == 0U)
        return destination;
    /* Copy backwards when the regions overlap with the destination above the
     * source, which is the case a forward loop would corrupt. */
    if (out < in) {
        for (size_t index = 0U; index < count; ++index)
            out[index] = in[index];
    } else {
        for (size_t index = count; index != 0U; --index)
            out[index - 1U] = in[index - 1U];
    }
    return destination;
}

void* memset(void* destination, int value, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto byte = static_cast<unsigned char>(static_cast<unsigned int>(value) & 0xffU);
    for (size_t index = 0U; index < count; ++index)
        out[index] = byte;
    return destination;
}

int memcmp(const void* left, const void* right, size_t count) noexcept {
    const auto* a = static_cast<const unsigned char*>(left);
    const auto* b = static_cast<const unsigned char*>(right);
    for (size_t index = 0U; index < count; ++index) {
        if (a[index] != b[index])
            return a[index] < b[index] ? -1 : 1;
    }
    return 0;
}

void* memchr(const void* haystack, int needle, size_t count) noexcept {
    const auto* bytes = static_cast<const unsigned char*>(haystack);
    const auto target = static_cast<unsigned char>(static_cast<unsigned int>(needle) & 0xffU);
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] == target)
            return const_cast<unsigned char*>(bytes + index);
    }
    return nullptr;
}

size_t strlen(const char* text) noexcept {
    size_t length = 0U;
    while (text[length] != '\0')
        ++length;
    return length;
}

size_t strnlen(const char* text, size_t limit) noexcept {
    size_t length = 0U;
    while (length < limit && text[length] != '\0')
        ++length;
    return length;
}

int strcmp(const char* left, const char* right) noexcept {
    size_t index = 0U;
    while (left[index] != '\0' && left[index] == right[index])
        ++index;
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (a == b)
        return 0;
    return a < b ? -1 : 1;
}

int strncmp(const char* left, const char* right, size_t limit) noexcept {
    for (size_t index = 0U; index < limit; ++index) {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        if (a != b)
            return a < b ? -1 : 1;
        if (a == '\0')
            return 0;
    }
    return 0;
}

char* strcpy(char* destination, const char* source) noexcept {
    size_t index = 0U;
    for (; source[index] != '\0'; ++index)
        destination[index] = source[index];
    destination[index] = '\0';
    return destination;
}

char* strncpy(char* destination, const char* source, size_t limit) noexcept {
    size_t index = 0U;
    for (; index < limit && source[index] != '\0'; ++index)
        destination[index] = source[index];
    /* POSIX pads with NULs rather than writing a single terminator, and
     * writes none at all when the source fills the buffer. */
    for (; index < limit; ++index)
        destination[index] = '\0';
    return destination;
}

char* strcat(char* destination, const char* source) noexcept {
    (void)strcpy(destination + strlen(destination), source);
    return destination;
}

char* strchr(const char* text, int character) noexcept {
    const auto target = static_cast<char>(character);
    for (const char* cursor = text;; ++cursor) {
        if (*cursor == target)
            return const_cast<char*>(cursor);
        if (*cursor == '\0')
            return nullptr;
    }
}

char* strrchr(const char* text, int character) noexcept {
    const auto target = static_cast<char>(character);
    const char* found = nullptr;
    for (const char* cursor = text;; ++cursor) {
        if (*cursor == target)
            found = cursor;
        if (*cursor == '\0')
            return const_cast<char*>(found);
    }
}

char* strstr(const char* haystack, const char* needle) noexcept {
    if (needle[0] == '\0')
        return const_cast<char*>(haystack);
    for (const char* start = haystack; *start != '\0'; ++start) {
        size_t index = 0U;
        while (needle[index] != '\0' && start[index] == needle[index])
            ++index;
        if (needle[index] == '\0')
            return const_cast<char*>(start);
    }
    return nullptr;
}

} // extern "C"
