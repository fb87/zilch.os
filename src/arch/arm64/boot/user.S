#if CONFIG_ROOT_ONLY_BOOT
.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
.incbin "out/build/arm64/qemu-arm64-virt/user/init.bin"
.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:
#else
.section .user_text,"ax"
.balign 4096
.global sys_arm64_user_image_start
sys_arm64_user_image_start:
  mov x19, x0                 // thread id / role
  mov x20, x1                 // deterministic seed
  mov x21, #0                 // iteration

  // Threads 0 and 1 are IPC servers on different CPUs.
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
  mov x1, #1                  // receive
  mov x8, #0                  // sys_ipc
  svc #0
.Lserver_loop:
  // Echo the four message registers and atomically reply+receive.
  mov x0, x22
  mov x1, #2                  // reply_receive
  mov x8, #0
  svc #0
  b .Lserver_loop

.Lclient_loop:
  eor x20, x20, x20, lsl #13
  eor x20, x20, x20, lsr #7
  eor x20, x20, x20, lsl #17
  add x21, x21, #1

  // One deterministic contained user fault.
  cmp x19, #4
  b.ne .Lclient_call
  cmp x21, #64
  b.ne .Lclient_call
  mov x9, xzr
  ldr x10, [x9]

.Lclient_call:
  // Even clients call server A, odd clients call server B.
  tst x19, #1
  mov x0, #10
  mov x9, #11
  csel x0, x0, x9, eq
  mov x1, #0                  // call
  mov x2, x19                 // message word 0
  mov x3, x21                 // message word 1
  mov x4, x20                 // message word 2
  mov x5, xzr                 // message word 3
  mov x6, xzr                 // clear fuzz discriminator for normal IPC
  mov x8, #0
  svc #0                      // blocks until server replies

  // Deterministic malformed-input fuzz call, immediate kernel validation.
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
  mov x8, #0
  svc #0
  b .Lclient_loop

.balign 4096
.global sys_arm64_user_image_end
sys_arm64_user_image_end:

#endif
