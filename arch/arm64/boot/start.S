.section .text.boot,"ax"
.global _start
.extern sys_kernel_entry
.extern __bss_start
.extern __bss_end
.extern __boot_stack_top

_start:
	mrs x0, CurrentEL
	cmp x0, #(2 << 2)
	b.ne 1f
	mov x0, #(1 << 31)
	msr hcr_el2, x0
	mov x0, #0x3c5
	msr spsr_el2, x0
	adr x0, 1f
	msr elr_el2, x0
	eret

1:
	ldr x0, =__boot_stack_top
	mov sp, x0
	ldr x0, =__bss_start
	ldr x1, =__bss_end

2: cmp x0, x1
	b.hs 3f
	str xzr, [x0], #8
	b 2b

3: bl sys_kernel_entry

4: wfe
	b 4b
