ENTRY(_start)
SECTIONS
{
    . = 0x0000000000400000;
    .text : ALIGN(0x1000) { *(.text.start) *(.text .text.*) }
    .rodata : ALIGN(0x1000) { *(.rodata .rodata.*) }
    /* Named explicitly for the same reason as arm64: an unnamed writable
     * orphan is packed behind .rodata without page alignment, and the ELF
     * loader rejects two present segments sharing one page. */
    .data : ALIGN(0x1000) {
        *(.data .data.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.got .got.plt)
    }
    .bss : ALIGN(0x1000) { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
