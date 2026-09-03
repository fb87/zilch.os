#include <sys/types.hh>

/*
 * The image bin/fork-probe execs into. It exists only to report a value no
 * other program returns, so observing 55 proves control actually reached a
 * different image rather than the forked child simply carrying on.
 *
 * It takes no arguments deliberately: exec discards the address space, and
 * this program's job is to prove it started, not to re-test the argument
 * path bin/argv-probe already covers.
 */
extern "C" int main(sys::word_t, sys::word_t) noexcept {
    return 55;
}
