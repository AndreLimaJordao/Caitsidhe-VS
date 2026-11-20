.section .text
.globl _start
_start:
    lui t0, %hi(tohost)
    addi t0, t0, %lo(tohost)
    li t1, 3
    sw t1, 0(t0)
again: j again
.section .data
.balign 4
.globl tohost
tohost: .word 0
