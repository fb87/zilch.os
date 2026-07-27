#include <sys/thread.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

extern "C" [[noreturn]] void sys_user_exit(sys::s32 status) noexcept {
    sys::thread_exit(static_cast<sys::word_t>(static_cast<sys::s64>(status)));
}
