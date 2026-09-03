#include <stdlib.h>
#include <string.h>

/*
 * The environment is whatever the spawner put in the argument block; the
 * runtime points this at it before main runs (see process_entry.cc). It is
 * null for a program started without a block, which getenv treats as an
 * empty environment rather than a fault.
 */
extern "C" char** environ = nullptr;

extern "C" char* getenv(const char* name) noexcept {
    if (environ == nullptr || name == nullptr)
        return nullptr;
    const size_t length = strlen(name);
    for (char** entry = environ; *entry != nullptr; ++entry) {
        /* A match is the name, then '=' -- comparing only the prefix would
         * make "PATHOLOGICAL=1" answer a lookup for "PATH". */
        if (strncmp(*entry, name, length) == 0 && (*entry)[length] == '=')
            return *entry + length + 1U;
    }
    return nullptr;
}
