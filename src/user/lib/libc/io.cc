#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/console_client.hh>
#include <sys/control.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

/*
 * File descriptors, and the process-control calls a shell needs.
 *
 * A descriptor is a local name for a capability plus a little state, not a
 * kernel object: this system has no file-descriptor concept, so the table
 * lives here, in the process. That is also why descriptors survive fork
 * automatically -- fork copies the cspace and the memory holding this table,
 * so the child sees the same descriptors naming the same capabilities,
 * without the kernel knowing what a descriptor is.
 *
 * File-backed descriptors are opened through the VFS server. Until a
 * process is given a capability to one, open() reports that there is no
 * such file rather than pretending to succeed.
 */
namespace
{
    namespace abi = sys::abi::v1;

    inline constexpr int descriptor_capacity = 16;

    enum class kind : unsigned char {
        closed = 0U,
        console_in,
        console_out,
        file,
    };

    struct descriptor {
        kind role;
        sys::word_t handle; // VFS handle for `file`; unused otherwise
    };

    descriptor table[descriptor_capacity] = {
        {kind::console_in, 0U},
        {kind::console_out, 0U},
        {kind::console_out, 0U},
    };

    /*
     * Child bookkeeping for waitpid. fork hands back a kernel thread id, but
     * process_wait needs the capability the fork installed, so the pid a
     * program sees is an index into this table rather than the thread id --
     * which also keeps pids stable if the kernel ever reuses a slot.
     */
    inline constexpr int child_capacity = 8;
    inline constexpr sys::capability_id_t child_selector_base = 200U;

    struct child_entry {
        bool live;
    };
    child_entry children[child_capacity]{};

    [[nodiscard]] bool valid(int descriptor_number) noexcept {
        return descriptor_number >= 0 && descriptor_number < descriptor_capacity;
    }
} // namespace

extern "C" {

ssize_t write(int descriptor_number, const void* buffer, size_t count) noexcept {
    if (!valid(descriptor_number) || buffer == nullptr)
        return -1;
    const descriptor entry = table[descriptor_number];
    if (entry.role != kind::console_out)
        return -1;
    const auto* bytes = static_cast<const char*>(buffer);
    for (size_t index = 0U; index < count; ++index) {
        /* Byte at a time: the console protocol's string form is capped at a
         * couple of dozen bytes, and a partial write would be worse than a
         * slow one. */
        if (!sys::native::ok(sys::console::write_byte(
                sys::native::stdout_endpoint, static_cast<sys::u8>(bytes[index]))))
            return index == 0U ? -1 : static_cast<ssize_t>(index);
    }
    return static_cast<ssize_t>(count);
}

ssize_t read(int descriptor_number, void* buffer, size_t count) noexcept {
    if (!valid(descriptor_number) || buffer == nullptr || count == 0U)
        return -1;
    const descriptor entry = table[descriptor_number];
    if (entry.role != kind::console_in)
        return -1;
    auto* bytes = static_cast<char*>(buffer);
    /*
     * Blocks until at least one byte arrives, then returns what is already
     * available rather than filling the buffer -- a line-oriented reader
     * would otherwise wait for bytes the user has not typed yet.
     */
    for (;;) {
        const auto reply = sys::console::read_byte(sys::native::stdin_endpoint);
        if (!reply.available)
            continue; // nothing typed yet
        bytes[0] = static_cast<char>(reply.value);
        return 1;
    }
}

int close(int descriptor_number) noexcept {
    if (!valid(descriptor_number))
        return -1;
    table[descriptor_number].role = kind::closed;
    return 0;
}

int dup2(int from, int to) noexcept {
    if (!valid(from) || !valid(to))
        return -1;
    table[to] = table[from];
    return to;
}

int open(const char*, int) noexcept {
    /* Filled in once the VFS server exists; reported as "no such file"
     * rather than a spurious success so callers take their error path. */
    return -1;
}

int fork() noexcept {
    for (int slot = 0; slot < child_capacity; ++slot) {
        if (children[slot].live)
            continue;
        sys::word_t identifier = 0U;
        const auto selector =
            static_cast<sys::capability_id_t>(child_selector_base + static_cast<unsigned>(slot));
        if (!sys::native::ok(sys::control_result1(
                identifier, abi::control_operation::process_fork, selector)))
            return -1;
        if (identifier == 0U)
            return 0; // the child
        children[slot].live = true;
        return slot + 1; // pids are 1-based so 0 stays unambiguous
    }
    return -1;
}

int execv(const char* path, char* const argv[]) noexcept {
    (void)path;
    (void)argv;
    /*
     * Needs a role bound to the named image, and role_image_bind is
     * root-gated, so a program cannot resolve a path for itself. This
     * becomes a request to the VFS/spawn service once that exists; until
     * then exec is reachable only through the raw process_exec operation
     * with an already-bound role.
     */
    return -1;
}

int waitpid(int pid, int* status, int) noexcept {
    const int slot = pid - 1;
    if (slot < 0 || slot >= child_capacity || !children[slot].live)
        return -1;
    const auto selector =
        static_cast<sys::capability_id_t>(child_selector_base + static_cast<unsigned>(slot));
    for (;;) {
        sys::word_t value = 0U;
        const sys::word_t result =
            sys::control_result1(value, abi::control_operation::process_wait, selector);
        if (result == static_cast<sys::word_t>(sys::error_t::busy))
            continue;
        if (!sys::native::ok(result))
            return -1;
        if (status != nullptr)
            *status = static_cast<int>(value);
        (void)sys::control(abi::control_operation::process_destroy, selector);
        children[slot].live = false;
        return pid;
    }
}

[[noreturn]] void _exit(int status) noexcept {
    (void)sys::control(abi::control_operation::thread_exit,
                       static_cast<sys::word_t>(static_cast<sys::s64>(status)));
    __builtin_unreachable();
}

[[noreturn]] void exit(int status) noexcept {
    _exit(status);
}

[[noreturn]] void abort() noexcept {
    _exit(127);
}

} // extern "C"
