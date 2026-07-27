#include <sys/ipc.hh>
#include <sys/syscall.hh>
#include <sys/types.hh>

#include <abi/sys/v1/ipc.hh>

namespace
{
    inline constexpr sys::word_t endpoint = 10U;
    inline constexpr sys::word_t fault_address = 0x20004000U;
    inline constexpr sys::word_t completion_magic = 0x50414745U;
} // namespace

extern "C" int main(sys::word_t role, sys::word_t) noexcept {
    const sys::word_t client_index = role - 0x101U;
    if (client_index >= 2U)
        return 1;
    volatile sys::word_t* page = reinterpret_cast<volatile sys::word_t*>(fault_address);
    const sys::word_t value = 0x5a494c4300000000ULL | client_index;
    *page = value;
    if (*page != value)
        return 2;
    const sys::word_t call =
        sys_ipc_invoke_raw(endpoint, static_cast<sys::word_t>(sys::abi::v1::ipc_operation::call),
                           completion_magic, fault_address, 1U << client_index, 0U, 0U, 0U);
    return call == static_cast<sys::word_t>(sys::error_t::success) ? 0 : 3;
}
