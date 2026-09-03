/* Hand-written thread-entry code, not generated -- unrelated to exception
 * vectoring despite living alongside vectors_low.S/vectors_high.S. Split
 * into its own file (rather than appended to vectors_high.S, where it
 * previously lived) because the auto-generated stub files are regenerated
 * by tools/kernel/gen_vectors.py, which has no reason to know about this. */

.section .text.vectors
.align 16
.global sys_amd64_enter_user
.type sys_amd64_enter_user, %function
sys_amd64_enter_user:
    /* RDI = pointer to frame_t */
    mov %rdi, %r8

    /* Build IRETQ frame on stack: SS, RSP, RFLAGS, CS, RIP */
    /* Load RSP (stack_pointer) from offset 160 in frame */
    mov 160(%r8), %rsp

    /* Push SS (offset 168) */
    mov 168(%r8), %rax
    push %rax

    /* Push RSP (stack_pointer from offset 160) */
    mov 160(%r8), %rax
    push %rax

    /* Push RFLAGS (status from offset 152) */
    mov 152(%r8), %rax
    push %rax

    /* Push CS (offset 144) */
    mov 144(%r8), %rax
    push %rax

    /* Push RIP (instruction_pointer from offset 136) */
    mov 136(%r8), %rax
    push %rax

    /* Load all GP registers from frame */
    mov 0(%r8), %rax
    mov 8(%r8), %rcx
    mov 16(%r8), %rdx
    mov 24(%r8), %rbx
    mov 32(%r8), %rbp
    mov 40(%r8), %rsi
    mov 48(%r8), %rdi
    mov 56(%r8), %r9
    mov 64(%r8), %r10
    mov 72(%r8), %r11
    mov 80(%r8), %r12
    mov 88(%r8), %r13
    mov 96(%r8), %r14
    mov 104(%r8), %r15

    /* Load r8 last */
    mov 112(%r8), %r8

    /* IRETQ jumps to user code */
    iretq
