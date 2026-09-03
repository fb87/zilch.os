#pragma once

namespace sys::arch::cpu
{
    inline void relax() noexcept {
        asm volatile("wfe");
    }

    /* Deliberately execute a guaranteed-undefined instruction, for testing
     * illegal-instruction fault delivery. 0x00000000 is UNDEFINED per the
     * ARM ARM at every exception level. Not [[noreturn]]: the caller does
     * not assume the fault handler never resumes this thread.
     */
    inline void trigger_illegal_instruction() noexcept {
        asm volatile(".inst 0x00000000");
    }
} // namespace sys::arch::cpu
