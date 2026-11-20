/* RV32I self-checking test with riscv-tests-style tohost protocol.
   The host reads a RAM location, pass=1, failure=3. */
.section .text
.globl _start
_start:
    li t0, 19
    li t1, 23
    add t2, t0, t1
    li t3, 42
    bne t2, t3, fail
    lui t0, %hi(tohost)
    addi t0, t0, %lo(tohost)
    li t1, 1
    sw t1, 0(t0)
pass: j pass
fail:
    lui t0, %hi(tohost)
    addi t0, t0, %lo(tohost)
    li t1, 3
    sw t1, 0(t0)
    j fail
.section .data
.balign 4
.globl tohost
tohost: .word 0
