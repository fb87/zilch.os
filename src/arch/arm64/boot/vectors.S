.section .text.vectors,"ax"

.macro vector_slot target
  b \target
  .space 124
.endm

.balign 2048
.global sys_arm64_vectors_el1
sys_arm64_vectors_el1:
  vector_slot el1_vector_0
  vector_slot el1_vector_1
  vector_slot el1_vector_2
  vector_slot el1_vector_3
  vector_slot el1_vector_4
  vector_slot el1_vector_5
  vector_slot el1_vector_6
  vector_slot el1_vector_7
  vector_slot el1_vector_8
  vector_slot el1_vector_9
  vector_slot el1_vector_10
  vector_slot el1_vector_11
  vector_slot el1_vector_12
  vector_slot el1_vector_13
  vector_slot el1_vector_14
  vector_slot el1_vector_15

.balign 2048
.global sys_arm64_vectors_el2
sys_arm64_vectors_el2:
  vector_slot el2_vector_0
  vector_slot el2_vector_1
  vector_slot el2_vector_2
  vector_slot el2_vector_3
  vector_slot el2_vector_4
  vector_slot el2_vector_5
  vector_slot el2_vector_6
  vector_slot el2_vector_7
  vector_slot el2_vector_8
  vector_slot el2_vector_9
  vector_slot el2_vector_10
  vector_slot el2_vector_11
  vector_slot el2_vector_12
  vector_slot el2_vector_13
  vector_slot el2_vector_14
  vector_slot el2_vector_15

.macro vector_stub name, common, number
\name:
  sub sp, sp, #16
  stp x16, x17, [sp]
  mov x16, #\number
  b \common
.endm

.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
  vector_stub el1_vector_\n, el1_common, \n
.endr
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
  vector_stub el2_vector_\n, el2_common, \n
.endr

.macro save_and_call level
  sub sp, sp, #288
  stp x0, x1, [sp, #0]
  stp x2, x3, [sp, #16]
  stp x4, x5, [sp, #32]
  stp x6, x7, [sp, #48]
  stp x8, x9, [sp, #64]
  stp x10, x11, [sp, #80]
  stp x12, x13, [sp, #96]
  stp x14, x15, [sp, #112]
  ldp x0, x1, [sp, #288]
  stp x0, x1, [sp, #128]
  stp x18, x19, [sp, #144]
  stp x20, x21, [sp, #160]
  stp x22, x23, [sp, #176]
  stp x24, x25, [sp, #192]
  stp x26, x27, [sp, #208]
  stp x28, x29, [sp, #224]
  str x30, [sp, #240]
  str x16, [sp, #248]
  mrs x0, sp_el0
  str x0, [sp, #256]
.if \level == 1
  mrs x0, elr_el1
  mrs x1, spsr_el1
.else
  mrs x0, elr_el2
  mrs x1, spsr_el2
.endif
  stp x0, x1, [sp, #264]
  mov x0, sp
  mov x1, #\level
  bl sys_arm64_exception_handler
  ldr x0, [sp, #256]
  msr sp_el0, x0
  ldp x0, x1, [sp, #264]
.if \level == 1
  msr elr_el1, x0
  msr spsr_el1, x1
.else
  msr elr_el2, x0
  msr spsr_el2, x1
.endif
  ldr x30, [sp, #240]
  ldp x28, x29, [sp, #224]
  ldp x26, x27, [sp, #208]
  ldp x24, x25, [sp, #192]
  ldp x22, x23, [sp, #176]
  ldp x20, x21, [sp, #160]
  ldp x18, x19, [sp, #144]
  ldp x16, x17, [sp, #128]
  ldp x14, x15, [sp, #112]
  ldp x12, x13, [sp, #96]
  ldp x10, x11, [sp, #80]
  ldp x8, x9, [sp, #64]
  ldp x6, x7, [sp, #48]
  ldp x4, x5, [sp, #32]
  ldp x2, x3, [sp, #16]
  ldp x0, x1, [sp, #0]
  add sp, sp, #304
  eret
.endm

.extern sys_arm64_exception_handler
el1_common:
  save_and_call 1
el2_common:
  save_and_call 2

.global sys_arm64_enter_el0
.type sys_arm64_enter_el0, %function
sys_arm64_enter_el0:
  mov x16, x0
  ldr x17, [x16, #256]
  msr sp_el0, x17
  ldp x17, x18, [x16, #264]
  msr elr_el1, x17
  msr spsr_el1, x18
  ldp x0, x1, [x16, #0]
  ldp x2, x3, [x16, #16]
  ldp x4, x5, [x16, #32]
  ldp x6, x7, [x16, #48]
  ldp x8, x9, [x16, #64]
  ldp x10, x11, [x16, #80]
  ldp x12, x13, [x16, #96]
  ldp x14, x15, [x16, #112]
  ldp x18, x19, [x16, #144]
  ldp x20, x21, [x16, #160]
  ldp x22, x23, [x16, #176]
  ldp x24, x25, [x16, #192]
  ldp x26, x27, [x16, #208]
  ldp x28, x29, [x16, #224]
  ldr x30, [x16, #240]
  mov x16, xzr
  mov x17, xzr
  eret
