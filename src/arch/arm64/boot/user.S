.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
  mov x19, x0
  mov x20, x1
  mov x21, #0
1:
  mov x22, x20
2:
  subs x22, x22, #1
  b.ne 2b
  add x21, x21, #1
  mov x0, #1
  mov x1, #0
  mov x2, #3
  mov x3, x19
  mov x4, x21
  mov x5, x20
  mov x8, #0
  svc #0
  b 1b
.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:
