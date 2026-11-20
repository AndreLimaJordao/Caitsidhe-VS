.section .text
.globl _start
_start:
    li t0, 40
    addi t1, t0, 2
    lui t2, %hi(begin_signature)
    addi t2, t2, %lo(begin_signature)
    sw t1, 0(t2)
    li t1, 0x55
    sw t1, 4(t2)
    ebreak
.section .data
.balign 4
.globl begin_signature
begin_signature: .word 0, 0
.globl end_signature
end_signature:
