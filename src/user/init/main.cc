#include <abi/sys/v1/control.hh>
#include <sys/control.hh>
#include <sys/types.hh>

extern "C" int main() noexcept
{
    /*
     * Standalone root-server ELF entry.  The current compatibility boot image
     * is still embedded for the SMP acceptance run, while this ELF validates
     * and freezes the Profile 1.0 userspace invocation ABI.  Once the earlyfs
     * ELF loader is enabled, this becomes the sole initial policy task.
     */
    const sys::word_t result = sys::control(
        sys::abi::v1::control_operation::notification_poll, 14U);
    return static_cast<int>(result < 0 ? 1 : 0);
}
