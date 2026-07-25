.section .text.boot,"ax"
.global _start
.global _secondary_start
.extern sys_kernel_entry
.extern sys_kernel_secondary_entry
.extern __bss_start
.extern __bss_end
.extern __cpu_stacks_start

.macro select_cpu_stack
  mrs x9, mpidr_el1
  and x9, x9, #0xff
  ldr x10, =__cpu_stacks_start
  add x9, x9, #1
  lsl x9, x9, #14
  add x10, x10, x9
  mov sp, x10
.endm

.macro enter_el1 target
  mrs x11, CurrentEL
  cmp x11, #(2 << 2)
  b.ne \target

  /* Keep the incoming EL2 vector table so firmware PSCI HVC remains usable. */
  /* EL1h uses SP_EL1, not the SP selected while executing at EL2. */
  mov x11, sp
  msr sp_el1, x11

  /* Enable the GIC system-register interface for EL1. */
  mov x11, #0xf
  msr ICC_SRE_EL2, x11
  isb

  msr cntvoff_el2, xzr
  mov x11, #(1 << 31)
  msr hcr_el2, x11
  mov x11, #0x3c5
  msr spsr_el2, x11
  adr x11, \target
  msr elr_el2, x11
  isb
  eret
.endm

_start:
  select_cpu_stack
  enter_el1 .Lboot_el1

.Lboot_el1:
  /* Re-select SP after EL transition so direct EL1 and EL2 boot agree. */
  select_cpu_stack

  ldr x0, =__bss_start
  ldr x1, =__bss_end
.Lclear_bss:
  cmp x0, x1
  b.hs .Lbss_done
  str xzr, [x0], #8
  b .Lclear_bss

.Lbss_done:
  bl sys_kernel_entry
.Lboot_halt:
  wfe
  b .Lboot_halt

_secondary_start:
  select_cpu_stack
  enter_el1 .Lsecondary_el1

.Lsecondary_el1:
  select_cpu_stack
  bl sys_kernel_secondary_entry
.Lsecondary_halt:
  wfe
  b .Lsecondary_halt
