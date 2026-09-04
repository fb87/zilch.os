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
    /*
     * Generic driver-section registry (docs/architecture/
     * KERNEL_ARCH_PLATFORM_DECOUPLING.md): every ARCH_INIT/PLAT_INIT
     * entry and every SYS_OPS-tagged ops-vtable singleton lands in one of
     * these two fixed section names, regardless of which driver or
     * stage. Adding or removing a driver never requires touching this
     * file again -- only these two rules, added once, are needed.
     *
     * The four literal tokens below (.sys_driver, .sys_ops,
     * __sys_drivers_start, __sys_drivers_end) must match
     * SYS_DRIVER_SECTION_NAME/SYS_OPS_SECTION_NAME/SYS_DRIVER_TABLE_BEGIN/
     * SYS_DRIVER_TABLE_END in src/kernel/include/sys/kernel/init/init.hh
     * exactly. This linker script is not preprocessed, so it cannot
     * expand those macros -- renaming any of them means updating this
     * file (and src/arch/amd64/kernel.ld) to match by hand.
     */
    __sys_drivers_start = .;
    KEEP(*(.sys_driver))
    __sys_drivers_end = .;
    KEEP(*(.sys_ops))
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
  }
  .noinit (NOLOAD) : ALIGN(4096) {
    *(.noinit .noinit.*)
    . = ALIGN(4096);
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
