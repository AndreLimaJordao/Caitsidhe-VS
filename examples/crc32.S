/* Feed "123456789" to the CRC32 device with two words and a byte. */
.section .text
.globl _start
_start:
    lui t0, 0x90003
    li t1, 0x34333231
    sw t1, 0(t0)
    li t1, 0x38373635
    sw t1, 0(t0)
    li t1, 0x39
    sb t1, 0(t0)
    lw t1, 4(t0)
    li t2, 0xcbf43926
    bne t1, t2, fail
    li t1, 1
    j finish
fail:
    li t1, 3
finish:
    la t0, tohost
    sw t1, 0(t0)
    j .
.section .data
.balign 4
.globl tohost
tohost:
    .word 0
