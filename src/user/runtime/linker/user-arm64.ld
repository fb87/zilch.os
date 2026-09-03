ENTRY(_start)
SECTIONS
{
    . = 0x0000000020000000;
    .text : ALIGN(0x1000) { *(.text.start) *(.text .text.*) }
    .rodata : ALIGN(0x1000) { *(.rodata .rodata.*) }
    /*
     * .got, .got.plt and .data.rel.ro must be named explicitly.
     *
     * An output section this script does not name is an orphan, and the
     * linker places orphans immediately after the section they most
     * resemble -- without the ALIGN(0x1000) the named sections get. A
     * writable orphan landing behind .rodata produces a second LOAD segment
     * sharing .rodata's page, and elf64::load_dynamic() rejects two present
     * segments touching one page (USR-014's overlap check). The image then
     * fails to load, reported only as "user object initialization failed".
     *
     * No program had emitted a .got before, because none had referenced a
     * weak symbol: taking the address of one materialises a GOT entry, so
     * the linker can leave it null when the symbol is undefined. The gap
     * was latent until the first code that did, and would have been hit by
     * any future use of a weak symbol, a function pointer table, or a
     * relocated constant.
     */
    .data : ALIGN(0x1000) {
        *(.data .data.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.got .got.plt)
    }
    .bss : ALIGN(0x1000) { *(.bss .bss.*) *(COMMON) }
    ASSERT(. <= 0x0000000020040000, "ARM64 userspace image exceeds bootstrap mapping window")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
