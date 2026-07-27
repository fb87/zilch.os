OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
  . = 0x40080000;
  .text : ALIGN(4096) {
    __kernel_text_start = .;
    KEEP(*(.text.boot))
    KEEP(*(.text.vectors))
    *(.text .text.*)
    . = ALIGN(4096);
    __kernel_text_end = .;
  }
  .user_text : ALIGN(4096) {
    __kernel_rodata_start = .;
    KEEP(*(.user_text))
  }
  .rodata : ALIGN(4096) {
    *(.rodata .rodata.*)
    . = ALIGN(4096);
    __kernel_rodata_end = .;
  }
  .data : ALIGN(4096) {
    __kernel_data_start = .;
    *(.data .data.*)
  }
  .bss (NOLOAD) : ALIGN(4096) {
    __bss_start = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(4096);
    __bss_end = .;
    __kernel_data_end = .;
  }
  .stacks (NOLOAD) : ALIGN(2097152) {
    __cpu_stacks_start = .;
    . += 4 * 0x10000;
    __cpu_stacks_end = .;
  }
  .hypervisor_stacks (NOLOAD) : ALIGN(65536) {
    __hypervisor_stacks_start = .;
    . += 4 * 0x10000;
    __hypervisor_stacks_end = .;
  }
  ASSERT((__hypervisor_stacks_end - __cpu_stacks_start) <= 0x200000,
         "EL1/EL2 stack slots must fit in one guarded L3 window")
  __kernel_end = ALIGN(4096);
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.init_array*) *(.fini_array*) }
}
