OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS { . = 0x00100000; .multiboot : { KEEP(*(.multiboot)) } .text : ALIGN(4096) { KEEP(*(.text.boot)) *(.text .text.*) } .rodata : ALIGN(4096) { *(.rodata .rodata.*) } .data : ALIGN(4096) { *(.data .data.*) } .bss (NOLOAD) : ALIGN(4096) { __bss_start = .; *(.bss .bss.* COMMON) __bss_end = .; } .stack (NOLOAD) : ALIGN(16) { . += 0x10000; __boot_stack_top = .; } /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.init_array*) *(.fini_array*) } }
