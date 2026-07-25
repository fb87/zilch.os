#include <sys/types.h>

extern "C" int main() noexcept;
extern "C" [[noreturn]] void sys_user_exit(sys::s32 status) noexcept;

extern "C" [[noreturn]] void sys_user_entry() noexcept
{
    sys_user_exit(static_cast<sys::s32>(main()));
}
