/* A very small bare-metal cat. Wait for the timer, then say hello. */
.section .text
.globl _start
_start:
    lui t0, 0x90002           /* Timer */
    li t1, 3                 /* Reset and enable */
    sw t1, 8(t0)
    li t1, 12
wait:
    lw t2, 0(t0)
    bltu t2, t1, wait

    lui t0, 0x90000           /* UART TX */
    la t1, message
    lbu t2, 0(t1)
send:
    sb t2, 0(t0)
    addi t1, t1, 1
    lbu t2, 0(t1)
    bnez t2, send
    ebreak

.section .rodata
message:
    .asciz "Meow!\n"
