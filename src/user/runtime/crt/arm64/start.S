.section .text.start,"ax"
.global _start
.extern sys_user_entry
_start:
    bl sys_user_entry
1:
    wfe
    b 1b
