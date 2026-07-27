.section .text.boot,"ax"
.global _start
.global _secondary_start
.extern sys_kernel_entry
.extern sys_kernel_secondary_entry
.extern __bss_start
.extern __bss_end
.extern __cpu_stacks_start
.extern __hypervisor_stacks_start

.macro load_cpu_stack symbol, destination
  mrs x9, mpidr_el1
  and x9, x9, #0xff
  ldr x10, =\symbol
  add x9, x9, #1
  lsl x9, x9, #14
  add \destination, x10, x9
.endm

.macro select_kernel_stack
  load_cpu_stack __cpu_stacks_start, x10
  mov sp, x10
.endm

.macro enter_el1 target
  mrs x11, CurrentEL
  cmp x11, #(2 << 2)
  b.ne \target

  /* EL2 must use memory distinct from the live EL1 kernel stack. */
  load_cpu_stack __hypervisor_stacks_start, x10
  mov sp, x10

  /* EL1h uses its dedicated per-CPU kernel stack through SP_EL1. */
  load_cpu_stack __cpu_stacks_start, x10
  msr sp_el1, x10

  /* Zilch owns EL2 vectors; platform PSCI remains available through SMC. */
  adr x11, sys_arm64_vectors_el2
  msr vbar_el2, x11
  isb

  /* Enable the GIC system-register interface for EL1. */
  mov x11, #0xf
  msr ICC_SRE_EL2, x11
  isb

  msr cntvoff_el2, xzr
  mov x11, #3
  msr cnthctl_el2, x11
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
  enter_el1 .Lboot_el1

.Lboot_el1:
  mov x19, x0
  /* Preserve the firmware DTB pointer across BSS clearing. */

  /* Direct EL1 boot and EL2 boot converge on the kernel stack. */
  select_kernel_stack

  ldr x0, =__bss_start
  ldr x1, =__bss_end
.Lclear_bss:
  cmp x0, x1
  b.hs .Lbss_done
  str xzr, [x0], #8
  b .Lclear_bss

.Lbss_done:
  mov x0, x19
  bl sys_kernel_entry
.Lboot_halt:
  wfe
  b .Lboot_halt

_secondary_start:
  enter_el1 .Lsecondary_el1

.Lsecondary_el1:
  select_kernel_stack
  bl sys_kernel_secondary_entry
.Lsecondary_halt:
  wfe
  b .Lsecondary_halt
