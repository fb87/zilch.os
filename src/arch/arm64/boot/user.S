.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
  mov x19, x0                 // thread id / role
  mov x20, x1                 // deterministic seed
  mov x21, #0                 // iteration

.Lloop:
  // xorshift64(seed)
  eor x20, x20, x20, lsl #13
  eor x20, x20, x20, lsr #7
  eor x20, x20, x20, lsl #17
  add x21, x21, #1

  // Thread 4 deliberately generates one unmapped EL0 data abort.
  cmp x19, #4
  b.ne .Lselect_case
  cmp x21, #64
  b.ne .Lselect_case
  mov x9, xzr
  ldr x10, [x9]

.Lselect_case:
  // Default: valid fuzz endpoint call.
  mov x0, #2
  mov x1, #0
  mov x2, #0
  mov x3, x19

  cmp x19, #1
  b.ne .Lcase2
  mov x0, #127                // invalid capability
  mov x2, #1
  b .Linvoke
.Lcase2:
  cmp x19, #2
  b.ne .Lcase3
  mov x1, #15                 // invalid operation
  mov x2, #2
  b .Linvoke
.Lcase3:
  cmp x19, #3
  b.ne .Lcase5
  add x3, x19, #1             // wrong identity
  mov x2, #3
  b .Linvoke
.Lcase5:
  cmp x19, #5
  b.ne .Lcase6
  mov x2, #4                  // valid randomized payload
  b .Linvoke
.Lcase6:
  cmp x19, #6
  b.ne .Lcase7
  mov x0, #-1                 // boundary capability
  mov x2, #5
  b .Linvoke
.Lcase7:
  cmp x19, #7
  b.lo .Linvoke
  // Mixed deterministic workload based on low random bits.
  and x9, x20, #3
  mov x2, #6
  cmp x9, #0
  b.eq .Linvoke
  cmp x9, #1
  b.ne 1f
  mov x0, #127
  b .Linvoke
1:
  cmp x9, #2
  b.ne 2f
  mov x1, #15
  b .Linvoke
2:
  add x3, x19, #1

.Linvoke:
  mov x4, x21
  mov x5, x20
  mov x6, #0x5a5a
  movk x6, #0x4655, lsl #16
  movk x6, #0x5a, lsl #32
  mov x8, #0
  svc #0
  b .Lloop

.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:
