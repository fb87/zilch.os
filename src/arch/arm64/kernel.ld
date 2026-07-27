OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
  . = 0x40080000;
  .text : ALIGN(4096) { KEEP(*(.text.boot)) KEEP(*(.text.vectors)) *(.text .text.*) }
  .user_text : ALIGN(4096) { KEEP(*(.user_text)) }
  .rodata : ALIGN(4096) { *(.rodata .rodata.*) }
  .data : ALIGN(4096) { *(.data .data.*) }
  .bss (NOLOAD) : ALIGN(4096) { __bss_start = .; *(.bss .bss.* COMMON) __bss_end = .; }
  .stacks (NOLOAD) : ALIGN(32768) {
    __cpu_stacks_start = .;
    . += 4 * 0x8000;
    __cpu_stacks_end = .;
  }
  .hypervisor_stacks (NOLOAD) : ALIGN(32768) {
    __hypervisor_stacks_start = .;
    . += 4 * 0x8000;
    __hypervisor_stacks_end = .;
  }
  __kernel_end = ALIGN(4096);
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.init_array*) *(.fini_array*) }
}
