#include <sys/control.hh>
#include <sys/types.hh>

#include <abi/sys/v1/control.hh>

/*
 * Covers fork and exec from inside a real process, reporting by exit status
 * so a regression names which link broke.
 *
 * Two rounds, because they prove different things and one cannot show both:
 * a forked child that execs has discarded the memory a copy check would
 * have looked at.
 *
 *   round 1  fork, and have the child confirm it is running on a private
 *            copy of the parent's memory -- it observes what the parent
 *            wrote before forking, then writes a different value that the
 *            parent must NOT see.
 *   round 2  fork, and have the child exec a different image, proving the
 *            address space is genuinely replaced.
 */
namespace
{
    namespace abi = sys::abi::v1;

    inline constexpr sys::word_t success = static_cast<sys::word_t>(sys::error_t::success);
    inline constexpr sys::word_t busy = static_cast<sys::word_t>(sys::error_t::busy);

    /* Two selectors so the second fork does not have to wait for the first
     * child's slot to be released. Both are clear of the bootstrap set a
     * process is born with (1-4, 10, 11, 14, 15, 16). */
    inline constexpr sys::capability_id_t first_child = 40U;
    inline constexpr sys::capability_id_t second_child = 41U;

    /* Bound by root to bin/exec-probe before this program is spawned; see
     * root_graph.hh's exec_probe_role, which must match. */
    inline constexpr sys::word_t exec_role = 0x407U;

    /* Written before forking and re-checked in the child. Volatile so the
     * compiler cannot assume it knows the value across the fork syscall. */
    volatile sys::word_t witness = 0U;

    [[nodiscard]] sys::word_t wait_for(sys::capability_id_t selector, sys::word_t& status) noexcept {
        for (sys::word_t attempt = 0U; attempt < 60000U; ++attempt) {
            const sys::word_t result =
                sys::control_result1(status, abi::control_operation::process_wait, selector);
            if (result != busy)
                return result;
        }
        return busy;
    }
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    witness = 0x5a5aU;

    sys::word_t child = 0U;
    const sys::word_t forked =
        sys::control_result1(child, abi::control_operation::process_fork, first_child);
    if (forked != success) {
        /* Encode the errno so a failure names the reason, not just the
         * call site: 30 + the negated error_t (1..7). */
        const sys::s64 code = -static_cast<sys::s64>(forked);
        return 30 + static_cast<int>(code & 0xf);
    }
    if (child == 0U) {
        /* Child of round 1. Both halves matter: seeing the parent's value
         * proves the copy happened, and the parent later not seeing this
         * write proves the copy is private rather than shared. */
        if (witness != 0x5a5aU)
            return 21;
        witness = 0x9999U;
        return 7;
    }

    sys::word_t status = 0U;
    if (wait_for(first_child, status) != success)
        return 22;
    if (status != 7U)
        return 23;
    if (witness != 0x5a5aU)
        return 24; // the child's write leaked into the parent
    (void)sys::control(abi::control_operation::process_destroy, first_child);

    sys::word_t second = 0U;
    if (sys::control_result1(second, abi::control_operation::process_fork, second_child) != success)
        return 25;
    if (second == 0U) {
        /* Child of round 2. On success this never returns -- the thread
         * resumes at the new image's entry point -- so reaching the return
         * means exec failed and left us running. */
        (void)sys::control(abi::control_operation::process_exec, exec_role);
        return 26;
    }

    if (wait_for(second_child, status) != success)
        return 27;
    if (status != 55U)
        return 28; // exec did not reach bin/exec-probe
    (void)sys::control(abi::control_operation::process_destroy, second_child);
    return 44;
}
