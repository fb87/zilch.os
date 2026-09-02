/* SPDX-License-Identifier: Apache-2.0 */
OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(sys_arm64_vpl011_guest_start)
SECTIONS
{
    . = 0;
    .text : ALIGN(4096) { *(.text .text.*) }
    .rodata : ALIGN(4096) { *(.rodata .rodata.*) }
    .data : ALIGN(4096) { *(.data .data.*) }
    .bss (NOLOAD) : ALIGN(4096) { *(.bss .bss.* COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
