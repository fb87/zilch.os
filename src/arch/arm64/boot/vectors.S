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

vector_stub el1_vector_0, el1_common, 0
vector_stub el1_vector_1, el1_common, 1
vector_stub el1_vector_2, el1_common, 2
vector_stub el1_vector_3, el1_common, 3
vector_stub el1_vector_4, el1_common, 4
vector_stub el1_vector_5, el1_common, 5
vector_stub el1_vector_6, el1_common, 6
vector_stub el1_vector_7, el1_common, 7
vector_stub el1_vector_8, el1_common, 8
vector_stub el1_vector_9, el1_common, 9
vector_stub el1_vector_10, el1_common, 10
vector_stub el1_vector_11, el1_common, 11
vector_stub el1_vector_12, el1_common, 12
vector_stub el1_vector_13, el1_common, 13
vector_stub el1_vector_14, el1_common, 14
vector_stub el1_vector_15, el1_common, 15

vector_stub el2_vector_0, el2_common, 0
vector_stub el2_vector_1, el2_common, 1
vector_stub el2_vector_2, el2_common, 2
vector_stub el2_vector_3, el2_common, 3
vector_stub el2_vector_4, el2_common, 4
vector_stub el2_vector_5, el2_common, 5
vector_stub el2_vector_6, el2_common, 6
vector_stub el2_vector_7, el2_common, 7
vector_stub el2_vector_8, el2_common, 8
vector_stub el2_vector_9, el2_common, 9
vector_stub el2_vector_10, el2_common, 10
vector_stub el2_vector_11, el2_common, 11
vector_stub el2_vector_12, el2_common, 12
vector_stub el2_vector_13, el2_common, 13
vector_stub el2_vector_14, el2_common, 14
vector_stub el2_vector_15, el2_common, 15

.macro save_and_call level
  sub sp, sp, #272
  stp x0, x1, [sp, #0]
  stp x2, x3, [sp, #16]
  stp x4, x5, [sp, #32]
  stp x6, x7, [sp, #48]
  stp x8, x9, [sp, #64]
  stp x10, x11, [sp, #80]
  stp x12, x13, [sp, #96]
  stp x14, x15, [sp, #112]
  ldp x0, x1, [sp, #272]
  stp x0, x1, [sp, #128]
  stp x18, x19, [sp, #144]
  stp x20, x21, [sp, #160]
  stp x22, x23, [sp, #176]
  stp x24, x25, [sp, #192]
  stp x26, x27, [sp, #208]
  stp x28, x29, [sp, #224]
  str x30, [sp, #240]
  str x16, [sp, #248]
  mov x0, sp
  mov x1, #\level
  bl sys_arm64_exception_handler
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
  add sp, sp, #288
  eret
.endm

.extern sys_arm64_exception_handler
el1_common:
  save_and_call 1
el2_common:
  save_and_call 2
