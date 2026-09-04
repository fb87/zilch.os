#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.hh>

/*
 * Proves the VFS server end to end, from inside a real process: a real
 * ext2 read against a fixture built into the disk image
 * (tools/image/rootfs/etc/motd, packed there by the disk-image build), and
 * a /tmp round trip through ramfs.
 *
 * The ext2 checks are optional in a strict sense -- a machine booted with
 * no disk attached correctly finds nothing to mount, the same principle
 * root_graph.hh's block_check::absent already establishes -- but this probe
 * always has a real disk under normal certification and smoke runs, so it
 * treats "the fixture did not open" as a real failure rather than silently
 * passing. A machine legitimately run with no disk is expected to fail this
 * probe, which is the correct signal for that configuration.
 */
namespace
{
    inline constexpr const char* fixture_content = "zilch vfs fixture\n";
} // namespace

extern "C" int main(sys::word_t, sys::word_t) noexcept {
    // --- ext2, read-only ---
    const int motd = open("/etc/motd", O_RDONLY);
    if (motd < 0)
        return 10;
    char buffer[64];
    (void)memset(buffer, 0, sizeof(buffer));
    const ssize_t read_count = read(motd, buffer, sizeof(buffer) - 1U);
    if (read_count <= 0)
        return 11;
    if (strcmp(buffer, fixture_content) != 0)
        return 12;
    // A second read past the file's end must report end of file (0), not
    // an error and not stale bytes from the first read.
    const ssize_t tail = read(motd, buffer, sizeof(buffer));
    if (tail != 0)
        return 13;
    if (close(motd) != 0)
        return 14;

    const int missing = open("/etc/does-not-exist", O_RDONLY);
    if (missing >= 0)
        return 15; // a nonexistent path must not open successfully

    // --- /tmp, writable ---
    const int scratch = open("/tmp/probe.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (scratch < 0)
        return 20;
    const char* const payload = "roundtrip";
    if (write(scratch, payload, strlen(payload)) != static_cast<ssize_t>(strlen(payload)))
        return 21;
    if (close(scratch) != 0)
        return 22;

    const int reopened = open("/tmp/probe.txt", O_RDONLY);
    if (reopened < 0)
        return 23;
    char roundtrip[16];
    (void)memset(roundtrip, 0, sizeof(roundtrip));
    if (read(reopened, roundtrip, sizeof(roundtrip) - 1U) != static_cast<ssize_t>(strlen(payload)))
        return 24;
    if (strcmp(roundtrip, payload) != 0)
        return 25;
    if (close(reopened) != 0)
        return 26;

    printf("vfs-probe: %s ok\n", "checks");
    return 77;
}
