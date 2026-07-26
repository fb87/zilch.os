.section .rodata.guest_image,"a"
.balign 16
.global sys_arm64_guest_image_start
.global sys_arm64_guest_image_end

.macro guest_putc value
    mov x0, #1
    mov x1, #\value
    hvc #0
.endm

sys_arm64_guest_image_start:
    guest_putc '['
    guest_putc 'G'
    guest_putc 'U'
    guest_putc 'E'
    guest_putc 'S'
    guest_putc 'T'
    guest_putc ']'
    guest_putc ' '
    guest_putc 'b'
    guest_putc 'o'
    guest_putc 'o'
    guest_putc 't'
    guest_putc 10

    mov x0, #2
    hvc #0
    mov x20, x0

    guest_putc '['
    guest_putc 'G'
    guest_putc 'U'
    guest_putc 'E'
    guest_putc 'S'
    guest_putc 'T'
    guest_putc ']'
    guest_putc ' '
    guest_putc 't'
    guest_putc 'i'
    guest_putc 'm'
    guest_putc 'e'
    guest_putc 10

    mov x0, #4
    mov x1, x20
    hvc #0
1:  wfe
    b 1b
sys_arm64_guest_image_end:
