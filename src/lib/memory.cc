#include <sys/types.hh>

extern "C" void* memset(void* dst, int value, sys::usize_t n) noexcept {
    auto* p = static_cast<unsigned char*>(dst);
    for (sys::usize_t i = 0; i < n; ++i) {
        p[i] = static_cast<unsigned char>(value);
    }
    return dst;
}

extern "C" void* memcpy(void* dst, const void* src, sys::usize_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dst);
    auto* s = static_cast<const unsigned char*>(src);
    for (sys::usize_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dst;
}
