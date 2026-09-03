#pragma once

#include <sys/types.hh>

/*
 * NOT a real target architecture -- this tree exists solely so
 * tools/verification/run_host_kernel_logic.sh can compile and natively run
 * portable kernel logic (capability rights math, scheduling context math,
 * control-plane policy) on the machine running the test, whatever its CPU
 * architecture is, with ASan/UBSan attached.
 *
 * That test never calls anything that actually invokes current_id()/relax()
 * (they are reachable only through sys::kernel::object::table.hh's dynamic
 * object registration, which the test's three property functions never
 * exercise) -- but the header must still compile, because a transitively
 * included plain inline function's body is type-checked whether or not it
 * is ever called.
 *
 * Before this existed, that script hardcoded -I.../arch/amd64/include
 * regardless of host. amd64's cpu.hh uses `cpuid`, an unprivileged x86
 * instruction, so this worked by accident on an x86_64 host (which is what
 * CI runs on) but failed to even ASSEMBLE on an aarch64 host, since "=a" is
 * not a valid register-class constraint for that target. Simply matching
 * the host's own native arch tree instead would trade that build failure
 * for a worse one: arm64's cpu.hh reads MPIDR_EL1 via `mrs`, which is EL1
 * kernel-only -- an ordinary Linux process attempting it takes SIGILL, so
 * the test would compile cleanly and then crash on any real aarch64 host.
 * A dedicated, hardware-free implementation avoids both failure modes by
 * construction, matching what this test actually is: a test of logic, not
 * of any particular CPU.
 */
namespace sys::arch::cpu
{
    inline void initialize_boot_cpu() noexcept {}

    [[nodiscard]] inline cpu_id_t current_id() noexcept {
        return 0U;
    }

    inline void relax() noexcept {}

    inline void wait_for_event() noexcept {}

    [[noreturn]] inline void halt() noexcept {
        __builtin_trap();
    }
} // namespace sys::arch::cpu
