#include <sys/control.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>
#include <abi/sys/v1/process.hh>

/*
 * Two entry shapes coexist deliberately.
 *
 * Servers are started with (role, 0) and define the two-word main(); a shell
 * command is started with (role, args_address) and defines the POSIX one.
 * Which of the two a program gets is decided by whether its spawner mapped
 * an argument block, not by a build flag, so both kinds of program link
 * against this same runtime. A program that defines neither fails to link,
 * which is the intended outcome.
 */
extern "C" [[gnu::weak]] int main(sys::word_t argument0, sys::word_t argument1) noexcept;
extern "C" [[noreturn]] void sys_user_exit(sys::s32 status) noexcept;

namespace
{
    using sys::abi::v1::process_args_header;

    /*
     * argv/envp point into the mapped block itself; only the pointer arrays
     * live here. They are function-scope statics rather than stack arrays
     * because main() may keep them for its whole run, and because the entry
     * frame is the shallowest point in the process -- spending a kilobyte of
     * the stack here would be spending it for the program's entire lifetime.
     */
    char* argv_storage[sys::abi::v1::process_args_max_entries + 1U]{};
    char* envp_storage[sys::abi::v1::process_args_max_entries + 1U]{};

    /*
     * Confirms the string at `offset` is NUL-terminated inside the block.
     * valid_process_args() checks that every offset is in range, but not
     * that a string actually ends before the block does -- a final entry
     * running off the end is exactly the case offsets cannot rule out.
     */
    [[nodiscard]] bool terminated(const char* bytes, sys::u32 size, sys::u32 offset) noexcept {
        for (sys::u32 index = offset; index < size; ++index) {
            if (bytes[index] == '\0')
                return true;
        }
        return false;
    }

    /*
     * Maps the argument block the spawner minted. Retries because the
     * spawner's mint happens after process_create returns and this thread
     * can reach here first -- the same bounded probe every root-minted
     * capability needs. Only programs that take arguments run this, so a
     * server never pays for a capability it was never going to be given.
     */
    [[nodiscard]] bool map_args() noexcept {
        const sys::word_t attrs = sys::abi::v1::encode_mapping_attributes(
            sys::abi::v1::memory_type::normal, sys::abi::v1::memory_shareability::inner_shareable);
        return sys::native::retry([&]() noexcept {
            return sys::native::ok(sys::control(
                sys::abi::v1::control_operation::map_frame, sys::native::own_space,
                sys::native::args_frame, sys::native::args_address,
                static_cast<sys::word_t>(sys::abi::v1::CapabilityRight::read), attrs));
        });
    }

    [[nodiscard]] bool build_vectors(sys::word_t address, int& argc) noexcept {
        auto* const header = reinterpret_cast<process_args_header*>(address);
        if (!sys::abi::v1::valid_process_args(*header))
            return false;
        auto* const bytes = reinterpret_cast<char*>(address) + header->bytes_offset;
        const sys::u32 total = header->argc + header->envc;
        for (sys::u32 index = 0U; index < total; ++index) {
            if (!terminated(bytes, header->bytes_size, header->entries[index]))
                return false;
        }
        for (sys::u32 index = 0U; index < header->argc; ++index)
            argv_storage[index] = bytes + header->entries[index];
        for (sys::u32 index = 0U; index < header->envc; ++index)
            envp_storage[index] = bytes + header->entries[header->argc + index];
        argv_storage[header->argc] = nullptr;
        envp_storage[header->envc] = nullptr;
        argc = static_cast<int>(header->argc);
        return true;
    }
} // namespace

/*
 * Defined by programs that take arguments. Weak so that servers, which do
 * not define it, still link: a program is one shape or the other, and the
 * null check below is what selects between them at runtime.
 */
extern "C" [[gnu::weak]] int sys_user_main(int argc, char** argv, char** envp) noexcept;

/* libc's getenv() reads this; weak so a program linking no libc still
 * links. */
extern "C" [[gnu::weak]] char** environ;

extern "C" [[noreturn]] void sys_user_entry(sys::word_t argument0, sys::word_t argument1) noexcept {
    /*
     * Which main a program defines decides the shape, not whether a block
     * happened to be supplied: a program taking argv started with no block
     * gets an empty argv, rather than falling through to a two-word main()
     * it does not define.
     */
    if (sys_user_main != nullptr) {
        int argc = 0;
        if (map_args() && !build_vectors(sys::native::args_address, argc)) {
            /*
             * A malformed block is the spawner's bug, not the program's.
             * Running on a half-built argv would relocate the failure into
             * the program, where it is far harder to recognise.
             */
            sys_user_exit(127);
        }
        if (&environ != nullptr)
            environ = envp_storage;
        sys_user_exit(static_cast<sys::s32>(sys_user_main(argc, argv_storage, envp_storage)));
    }
    if (main != nullptr)
        sys_user_exit(static_cast<sys::s32>(main(argument0, argument1)));
    sys_user_exit(127);
}
