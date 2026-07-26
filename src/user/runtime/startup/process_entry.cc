#include <sys/types.hh>

extern "C" int main(sys::word_t argument0, sys::word_t argument1) noexcept;
extern "C" [[noreturn]] void sys_user_exit(sys::s32 status) noexcept;

extern "C" [[noreturn]] void sys_user_entry(sys::word_t argument0,
                                             sys::word_t argument1) noexcept
{
    sys_user_exit(static_cast<sys::s32>(main(argument0, argument1)));
}
