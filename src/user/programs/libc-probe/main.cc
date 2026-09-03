#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Exercises the libc from inside a real process and reports by exit status,
 * with a distinct code per check so a regression names what broke.
 *
 * The heap checks matter most: malloc here is not a library detail but the
 * first thing in this system to grow a process's address space at runtime,
 * by creating frames and mapping them above the stack. Nothing before Stage
 * 1 could have done that at all.
 */
namespace
{
    [[nodiscard]] bool text_equals(const char* left, const char* right) noexcept {
        return strcmp(left, right) == 0;
    }
} // namespace

extern "C" int sys_user_main(int argc, char** argv, char** envp) noexcept {
    (void)envp;

    // --- string.h ---
    char buffer[32];
    (void)strcpy(buffer, "zilch");
    if (strlen(buffer) != 5U)
        return 10;
    if (!text_equals(buffer, "zilch"))
        return 11;
    (void)strcat(buffer, "-os");
    if (!text_equals(buffer, "zilch-os"))
        return 12;
    if (strncmp(buffer, "zilch", 5U) != 0 || strcmp(buffer, "zilch") == 0)
        return 13;
    if (strchr(buffer, '-') != buffer + 5)
        return 14;
    if (strstr(buffer, "ch-o") != buffer + 3)
        return 15;

    char copy[32];
    (void)memset(copy, 0, sizeof(copy));
    (void)memcpy(copy, buffer, strlen(buffer) + 1U);
    if (!text_equals(copy, buffer))
        return 16;
    if (memcmp(copy, buffer, strlen(buffer)) != 0)
        return 17;
    /* Overlapping move, the case a plain forward copy corrupts. */
    (void)memmove(copy + 2, copy, 6U);
    if (memcmp(copy, "zizilch", 7U) != 0)
        return 18;

    // --- conversions ---
    if (atoi("  -42xyz") != -42)
        return 20;
    if (strtol("0x1f", nullptr, 0) != 31)
        return 21;
    if (strtol("777", nullptr, 8) != 511)
        return 22;

    // --- snprintf ---
    char rendered[64];
    const int written = snprintf(rendered, sizeof(rendered), "%s=%d %04x %c%%", "n", -7, 255, 'Z');
    if (!text_equals(rendered, "n=-7 00ff Z%"))
        return 30;
    if (written != 12)
        return 31;
    /* Truncation must still terminate, and must report the length the
     * output would have had. */
    char tiny[5];
    if (snprintf(tiny, sizeof(tiny), "abcdefgh") != 8 || !text_equals(tiny, "abcd"))
        return 32;

    // --- heap ---
    void* first = malloc(64U);
    if (first == nullptr)
        return 40;
    (void)memset(first, 0xab, 64U);
    void* second = malloc(64U);
    if (second == nullptr || second == first)
        return 41;
    /* The first block must survive allocating the second. */
    if (static_cast<unsigned char*>(first)[63] != 0xabU)
        return 42;
    free(first);
    free(second);
    /* Freeing both should let a larger request reuse the coalesced space
     * rather than always growing the heap. */
    void* merged = malloc(96U);
    if (merged == nullptr)
        return 43;
    free(merged);

    auto* zeroed = static_cast<unsigned char*>(calloc(48U, 1U));
    if (zeroed == nullptr)
        return 44;
    for (int index = 0; index < 48; ++index) {
        if (zeroed[index] != 0U)
            return 45;
    }
    free(zeroed);

    /* An allocation spanning several pages proves the heap grows by mapping
     * more than one page at a time. */
    auto* large = static_cast<unsigned char*>(malloc(9000U));
    if (large == nullptr)
        return 46;
    large[0] = 1U;
    large[8999] = 2U;
    if (large[0] != 1U || large[8999] != 2U)
        return 47;
    free(large);

    auto* grown = static_cast<char*>(malloc(8U));
    if (grown == nullptr)
        return 48;
    (void)strcpy(grown, "keep");
    auto* bigger = static_cast<char*>(realloc(grown, 128U));
    if (bigger == nullptr || !text_equals(bigger, "keep"))
        return 49;
    free(bigger);

    // --- argv and environment ---
    if (argc != 2 || !text_equals(argv[0], "libc-probe") || !text_equals(argv[1], "run"))
        return 50;
    const char* home = getenv("SHELL");
    if (home == nullptr || !text_equals(home, "zilch"))
        return 51;
    if (getenv("SHELLX") != nullptr || getenv("SHEL") != nullptr)
        return 52; // prefix matching would wrongly answer these

    /* Finally, prove stdout actually reaches the console. */
    printf("libc-probe: %s ok\n", "checks");
    return 66;
}
