.section .text.start,"ax"
.global _start
.extern sys_user_entry
_start:
    andq $-16, %rsp
    call sys_user_entry
1:
    pause
    jmp 1b
