.section .text.guest_image,"ax"
.balign 4096
.global sys_arm64_guest_image_start
.global sys_arm64_guest_image_end

.equ GUEST_HVC_CONSOLE, 1
.equ GUEST_HVC_TIME, 2
.equ GUEST_HVC_IRQ_ACK, 3
.equ GUEST_HVC_SHUTDOWN, 4
.equ GUEST_HVC_REPORT, 5
.equ GUEST_HVC_DIAGNOSTIC, 6

.equ DIAG_PRE_ENABLE, 1
.equ DIAG_POST_ENABLE, 2
.equ DIAG_GUEST_FAULT, 3

.equ REPORT_MMU, 1
.equ REPORT_TIMER, 2
.equ REPORT_EL0, 4

.equ GUEST_L1, 0x8000
.equ GUEST_L2, 0x9000
.equ GUEST_L3, 0xa000
.equ GUEST_EL0_STACK, 0xe000
.equ GUEST_EL1_STACK, 0xf000

.macro guest_putc value
    mov x0, #GUEST_HVC_CONSOLE
    mov x1, #\value
    hvc #0
.endm

.macro guest_report value
    mov x0, #GUEST_HVC_REPORT
    mov x1, #\value
    hvc #0
.endm

.macro vector_slot target
    b \target
    .space 124
.endm

sys_arm64_guest_image_start:
    movz x18, #0x534d
    movk x18, #0x5054, lsl #16
    cmp x19, x18
    b.eq guest_smp_entry
    /* Earliest possible progress marker: MMU off, vectors not installed. */
    guest_putc '>'
    b guest_main

.org 0x800
.balign 2048
guest_vectors:
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_current_sync
    vector_slot guest_current_irq
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_lower_sync
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected
    vector_slot guest_unexpected

/* Current-EL synchronous exceptions are not expected in single-vCPU verification guest. */
guest_current_sync:
    mrs x2, esr_el1
    mrs x3, far_el1
    mrs x4, elr_el1
    mrs x5, spsr_el1
    mov x6, sp
    mrs x7, par_el1
    mov x0, #GUEST_HVC_DIAGNOSTIC
    mov x1, #DIAG_GUEST_FAULT
    hvc #0
    mov x0, #GUEST_HVC_SHUTDOWN
    mov x1, #0xee
    hvc #0
1:  wfe
    b 1b

/* HCR_EL2.VI injects a virtual IRQ directly into this vector. */
guest_current_irq:
    movz x18, #0x534d
    movk x18, #0x5054, lsl #16
    cmp x19, x18
    b.eq guest_smp_irq
    mov x0, #GUEST_HVC_IRQ_ACK
    hvc #0
    guest_report REPORT_TIMER
    eret

/* EL0 uses SVC #0 as the guest-kernel acceptance call and SVC #1 to stop. */
guest_lower_sync:
    mrs x9, esr_el1
    lsr x10, x9, #26
    cmp x10, #0x15
    b.ne guest_unexpected
    and x10, x9, #0xffff
    cbnz x10, guest_el0_shutdown
    guest_report REPORT_EL0
    mov x0, #0x42
    eret

guest_el0_shutdown:
    mov x0, #GUEST_HVC_SHUTDOWN
    mov x1, #0
    hvc #0
2:  wfe
    b 2b

guest_unexpected:
    mrs x2, esr_el1
    mrs x3, far_el1
    mrs x4, elr_el1
    mrs x5, spsr_el1
    mov x6, sp
    mrs x7, par_el1
    mov x0, #GUEST_HVC_DIAGNOSTIC
    mov x1, #DIAG_GUEST_FAULT
    hvc #0
    mov x0, #GUEST_HVC_SHUTDOWN
    mov x1, #0xef
    hvc #0
3:  wfe
    b 3b

.org 0x1800
guest_el0_entry:
    svc #0
    cmp x0, #0x42
    b.ne guest_el0_fail
    svc #1
guest_el0_fail:
    svc #1

.org 0x2000
guest_main:
    mov sp, #GUEST_EL1_STACK

    guest_putc '['
    guest_putc 'G'
    guest_putc 'U'
    guest_putc 'E'
    guest_putc 'S'
    guest_putc 'T'
    guest_putc ']'
    guest_putc ' '
    guest_putc 'b'
    guest_putc 'o'
    guest_putc 'o'
    guest_putc 't'
    guest_putc 10

    /* Build a three-level, 4 KiB, identity-mapped guest stage-1 hierarchy. */
    mov x0, #GUEST_L1
    mov x1, #GUEST_L2
    orr x1, x1, #3
    str x1, [x0]

    mov x0, #GUEST_L2
    mov x1, #GUEST_L3
    orr x1, x1, #3
    str x1, [x0]

    /*
     * Mirror the stage-2 W^X policy in stage 1:
     *   0x0000-0x3fff: EL1 read-only, executable
     *   0x4000-0xffff: EL1 read-write, PXN and UXN
     *
     * The code/vector pages use AP=0b10, AF=1, inner-shareable, normal memory.
     * The writable pages use AP=0b00 with both execute-never bits set.
     */
    mov x0, #GUEST_L3
    mov x2, #0
1:
    lsl x3, x2, #12
    cmp x2, #4
    b.hs 2f
    orr x3, x3, #3
    orr x3, x3, #(2 << 6)
    orr x3, x3, #(3 << 8)
    orr x3, x3, #(1 << 10)
    b 3f
2:
    orr x3, x3, #3
    orr x3, x3, #(3 << 8)
    orr x3, x3, #(1 << 10)
    mov x4, #(3 << 53)
    orr x3, x3, x4
3:
    str x3, [x0, x2, lsl #3]
    add x2, x2, #1
    cmp x2, #16
    b.lo 1b

    /* Table construction completed while stage 1 is disabled. */
    guest_putc 'T'

    mov x0, #0xff
    msr mair_el1, x0

    /*
     * 4 KiB granule, 32-bit VA input, 40-bit output, inner-shareable
     * write-back/write-allocate walks. Disable TTBR1 walks explicitly.
     */
    mov x0, #32
    orr x0, x0, #(1 << 23)
    orr x0, x0, #(2 << 32)
    orr x0, x0, #(3 << 12)
    orr x0, x0, #(1 << 10)
    orr x0, x0, #(1 << 8)
    msr tcr_el1, x0
    mov x0, #GUEST_L1
    msr ttbr0_el1, x0
    msr ttbr1_el1, xzr
    adr x0, guest_vectors
    msr vbar_el1, x0

    /* Publish tables and discard any stale EL1 translation state. */
    dsb ishst
    tlbi vmalle1
    dsb ish
    isb
    guest_putc 'C'

    /*
     * Probe the configured stage-1 translation before enabling SCTLR.M.
     * PAR_EL1.F == 0 proves that VA 0x2000 resolves through the new tables.
     */
    mov x0, #0x2000
    at s1e1r, x0
    isb
    mrs x1, par_el1
    tbnz x1, #0, guest_stage1_probe_failed
    mov x6, x1
    guest_putc 'P'

    /* Structured pre-enable snapshot: descriptors plus live EL1 state. */
    mov x2, #GUEST_L1
    ldr x2, [x2]
    mov x3, #GUEST_L2
    ldr x3, [x3]
    adr x4, guest_post_mmu_enable
    mov x5, sp
    /* x6 preserves PAR_EL1 across the 'P' console marker. */
    mrs x7, sctlr_el1
    mov x0, #GUEST_HVC_DIAGNOSTIC
    mov x1, #DIAG_PRE_ENABLE
    hvc #0

    /*
     * Enter the translated instruction stream through ERET rather than a
     * fall-through branch. ERET is a context-synchronizing event and reloads
     * PSTATE from a known EL1h, masked-interrupt state, avoiding ambiguity
     * around the first fetch after changing SCTLR_EL1.M.
     */
    adr x1, guest_post_mmu_enable
    msr elr_el1, x1
    mov x2, #0x3c5
    msr spsr_el1, x2
    mrs x0, sctlr_el1
    /* Stage 1 now has split RX and RW/XN mappings, so enable WXN safely. */
    orr x0, x0, #(1 << 19)
    orr x0, x0, #1
    msr sctlr_el1, x0
    isb
    eret

guest_post_mmu_enable:
    mrs x2, sctlr_el1
    mrs x3, tcr_el1
    mrs x4, ttbr0_el1
    adr x5, guest_post_mmu_enable
    mov x6, sp
    mrs x7, par_el1
    mov x0, #GUEST_HVC_DIAGNOSTIC
    mov x1, #DIAG_POST_ENABLE
    hvc #0
    guest_putc 'M'

    /* Enable data and instruction caches only after translated execution works. */
    mrs x0, sctlr_el1
    orr x0, x0, #(1 << 2)
    orr x0, x0, #(1 << 12)
    msr sctlr_el1, x0
    isb
    guest_putc 'K'
    b guest_stage1_enabled

guest_stage1_probe_failed:
    guest_putc 'F'
    mov x0, #GUEST_HVC_SHUTDOWN
    mov x1, #0xe1
    hvc #0
4:  wfe
    b 4b

guest_stage1_enabled:

    guest_report REPORT_MMU
    guest_putc '['
    guest_putc 'G'
    guest_putc 'U'
    guest_putc 'E'
    guest_putc 'S'
    guest_putc 'T'
    guest_putc ']'
    guest_putc ' '
    guest_putc 'm'
    guest_putc 'm'
    guest_putc 'u'
    guest_putc 10

    /*
     * Arm a far-future virtual timer before descheduling. The host must retain
     * CNTV_CTL_EL0 and CNTV_CVAL_EL0 across the WFI exit and subsequent run.
     * The certification harness injects HCR_EL2.VI independently of expiry.
     */
    mrs x0, cntvct_el0
    mov x1, #1
    lsl x1, x1, #40
    add x0, x0, x1
    msr cntv_cval_el0, x0
    mov x0, #1
    msr cntv_ctl_el0, x0
    isb
    msr daifclr, #2
    wfi

    guest_putc '['
    guest_putc 'G'
    guest_putc 'U'
    guest_putc 'E'
    guest_putc 'S'
    guest_putc 'T'
    guest_putc ']'
    guest_putc ' '
    guest_putc 't'
    guest_putc 'i'
    guest_putc 'm'
    guest_putc 'e'
    guest_putc 'r'
    guest_putc 10

    mov x0, #GUEST_EL0_STACK
    msr sp_el0, x0
    adr x0, guest_el0_entry
    msr elr_el1, x0
    mov x0, #0
    msr spsr_el1, x0
    eret

/*
 * Real SMP certification stream.  x19 contains "SMPT", x20 is the vCPU ID,
 * and x22 is the number of participants in this VM.  All synchronization and
 * interrupt accounting below is performed by guest instructions in shared
 * guest RAM.
 */
.org 0x3000
guest_smp_entry:
    mov sp, #GUEST_EL1_STACK
    mov x0, #0x800
    msr vbar_el1, x0
    isb

    mov x2, #0x5000
1:
    ldaxr w3, [x2]
    add w3, w3, #1
    stlxr w4, w3, [x2]
    cbnz w4, 1b
2:
    ldar w3, [x2]
    cmp w3, w22
    b.lo 2b

    mov x0, #GUEST_HVC_REPORT
    mov x1, #1
    lsl x1, x1, x20
    hvc #0

    /* First WFI is a bounded host scheduling exit. */
    wfi

    /* The resumed stream accepts one host-injected virtual interrupt. */
    msr daifclr, #2
    isb
    wfi

guest_smp_after_irq:
    mov x0, #GUEST_HVC_REPORT
    mov x1, #0x100
    lsl x1, x1, x20
    hvc #0
    mov x0, #GUEST_HVC_SHUTDOWN
    hvc #0
4:
    wfe
    b 4b

/*
 * Current-EL IRQ slot is VBAR + 0x280.  Acknowledge the virtual source,
 * increment a shared guest counter, and skip the interrupted WFI.
 */
.org 0x3280
guest_smp_irq:
    mov x0, #GUEST_HVC_IRQ_ACK
    hvc #0
    mov x2, #0x5008
5:
    ldaxr w3, [x2]
    add w3, w3, #1
    stlxr w4, w3, [x2]
    cbnz w4, 5b
    mrs x5, elr_el1
    add x5, x5, #4
    msr elr_el1, x5
    eret

.org 0x4000
sys_arm64_guest_image_end:
