OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
  . = 0x40080000;
  .text : ALIGN(4096) { KEEP(*(.text.boot)) KEEP(*(.text.vectors)) *(.text .text.*) }
  .rodata : ALIGN(4096) { *(.rodata .rodata.*) }
  .data : ALIGN(4096) { *(.data .data.*) }
  .bss (NOLOAD) : ALIGN(4096) { __bss_start = .; *(.bss .bss.* COMMON) __bss_end = .; }
  .stacks (NOLOAD) : ALIGN(16384) {
    __cpu_stacks_start = .;
    . += 4 * 0x4000;
    __cpu_stacks_end = .;
  }
  .hypervisor_stacks (NOLOAD) : ALIGN(16384) {
    __hypervisor_stacks_start = .;
    . += 4 * 0x4000;
    __hypervisor_stacks_end = .;
  }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.init_array*) *(.fini_array*) }
}
