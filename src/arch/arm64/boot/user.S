#if CONFIG_ROOT_ONLY_BOOT
.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
.incbin USER_BIN_PATH
.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:

.global sys_arm64_memory_server_image_start
sys_arm64_memory_server_image_start:
.incbin MEMORY_SERVER_BIN_PATH
.balign 4096
.global sys_arm64_memory_server_image_end
sys_arm64_memory_server_image_end:

.global sys_arm64_pager_client_image_start
sys_arm64_pager_client_image_start:
.incbin PAGER_CLIENT_BIN_PATH
.balign 4096
.global sys_arm64_pager_client_image_end
sys_arm64_pager_client_image_end:
#else
.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
  mov x19, x0
  mov x20, x1
  mov x21, #0
  cmp x19, #0
  b.eq .Lserver_a
  cmp x19, #1
  b.eq .Lserver_b
  b .Lclient_loop
.Lserver_a:
  mov x22, #10
  b .Lserver_receive
.Lserver_b:
  mov x22, #11
.Lserver_receive:
  mov x0, x22
  mov x1, #1
  mov x7, xzr
  mov x8, #0
  svc #0
.Lserver_loop:
  mov x0, x22
  mov x1, #2
  mov x7, xzr
  mov x8, #0
  svc #0
  b .Lserver_loop
.Lclient_loop:
  eor x20, x20, x20, lsl #13
  eor x20, x20, x20, lsr #7
  eor x20, x20, x20, lsl #17
  add x21, x21, #1
  cmp x19, #4
  b.ne .Lclient_call
  cmp x21, #64
  b.ne .Lclient_call
  mov x9, xzr
  ldr x10, [x9]
.Lclient_call:
  tst x19, #1
  mov x0, #10
  mov x9, #11
  csel x0, x0, x9, eq
  mov x1, #0
  mov x2, x19
  mov x3, x21
  mov x4, x20
  mov x5, xzr
  mov x6, xzr
  mov x7, xzr
  mov x8, #0
  svc #0
  mov x0, #2
  mov x1, #0
  mov x2, #0
  mov x3, x19
  cmp x19, #2
  b.ne .Lfuzz3
  mov x0, #127
  mov x2, #1
  b .Lfuzz_invoke
.Lfuzz3:
  cmp x19, #3
  b.ne .Lfuzz5
  mov x1, #15
  mov x2, #2
  b .Lfuzz_invoke
.Lfuzz5:
  cmp x19, #5
  b.ne .Lfuzz6
  add x3, x19, #1
  mov x2, #3
  b .Lfuzz_invoke
.Lfuzz6:
  cmp x19, #6
  b.ne .Lfuzz_mixed
  mov x0, #-1
  mov x2, #5
  b .Lfuzz_invoke
.Lfuzz_mixed:
  and x9, x20, #3
  mov x2, #6
  cmp x9, #0
  b.eq .Lfuzz_invoke
  cmp x9, #1
  b.ne 1f
  mov x0, #127
  b .Lfuzz_invoke
1:
  cmp x9, #2
  b.ne 2f
  mov x1, #15
  b .Lfuzz_invoke
2:
  add x3, x19, #1
.Lfuzz_invoke:
  mov x4, x21
  mov x5, x20
  mov x6, #0x5a5a
  movk x6, #0x4655, lsl #16
  movk x6, #0x5a, lsl #32
  mov x7, xzr
  mov x8, #0
  svc #0
  b .Lclient_loop
.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:
#endif
