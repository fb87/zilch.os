.set MBALIGN, 1<<0
.set MEMINFO, 1<<1
.set FLAGS, MBALIGN | MEMINFO
.set MAGIC, 0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot,"a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text.boot,"ax"
.code32
.global _start
.extern sys_kernel_entry
.extern __bss_start
.extern __bss_end
.extern __boot_stack_top
_start:
  cli
  mov $boot_stack32_top, %esp

  /* Clear bootstrap page tables. */
  xor %eax, %eax
  mov $pml4, %edi
  mov $(4096*3/4), %ecx
  rep stosl

  /* Identity-map the first 1 GiB with 2 MiB pages. */
  mov $pdpt, %eax
  or $0x3, %eax
  mov %eax, pml4
  mov $pd, %eax
  or $0x3, %eax
  mov %eax, pdpt
  xor %ecx, %ecx
1:
  mov %ecx, %eax
  shl $21, %eax
  or $0x83, %eax
  mov %eax, pd(,%ecx,8)
  movl $0, pd+4(,%ecx,8)
  inc %ecx
  cmp $512, %ecx
  jne 1b

  mov %cr4, %eax
  or $(1<<5), %eax
  mov %eax, %cr4
  mov $pml4, %eax
  mov %eax, %cr3
  mov $0xc0000080, %ecx
  rdmsr
  or $(1<<8), %eax
  wrmsr
  mov %cr0, %eax
  or $(1<<31), %eax
  mov %eax, %cr0
  lgdt gdt64_ptr
  ljmp $0x08, $long_mode_entry

.code64
long_mode_entry:
  mov $0x10, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %ss
  lea __boot_stack_top(%rip), %rsp
  xor %rbp, %rbp

  lea __bss_start(%rip), %rdi
  lea __bss_end(%rip), %rcx
  sub %rdi, %rcx
  xor %eax, %eax
  rep stosb
  call sys_kernel_entry
2: hlt
  jmp 2b

.align 8
gdt64:
  .quad 0
  .quad 0x00af9a000000ffff
  .quad 0x00af92000000ffff
gdt64_end:
gdt64_ptr:
  .word gdt64_end-gdt64-1
  .long gdt64

.section .bss,"aw",@nobits
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096
.align 16
boot_stack32: .skip 16384
boot_stack32_top:
