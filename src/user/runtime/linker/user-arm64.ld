ENTRY(_start)
SECTIONS
{
    . = 0x0000000020000000;
    .text : ALIGN(0x1000) { *(.text.start) *(.text .text.*) }
    .rodata : ALIGN(0x1000) { *(.rodata .rodata.*) }
    .data : ALIGN(0x1000) { *(.data .data.*) }
    .bss : ALIGN(0x1000) { *(.bss .bss.*) *(COMMON) }
    ASSERT(. <= 0x0000000020040000, "ARM64 userspace image exceeds bootstrap mapping window")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
