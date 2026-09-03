#pragma once

namespace sys::arch::cpu
{
    inline void relax() noexcept {
        asm volatile("pause");
    }

    /* Deliberately execute a guaranteed-undefined instruction, for testing
     * illegal-instruction fault delivery. UD2 (0F 0B) is Intel-guaranteed
     * to always raise #UD. Not [[noreturn]]: the caller does not assume the
     * fault handler never resumes this thread.
     */
    inline void trigger_illegal_instruction() noexcept {
        asm volatile("ud2");
    }
} // namespace sys::arch::cpu
