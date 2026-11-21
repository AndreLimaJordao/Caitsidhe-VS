/* The cat learned to stop polling the Timer. */
.section .text
.globl _start
_start:
    la t0, trap_handler
    csrw mtvec, t0
ecall_site:
    ecall
    li t0, 11
    bne s1, t0, fail
break_site:
    ebreak
    li t0, 3
    bne s1, t0, fail
    lui t0, 0x40000
bad_load_site:
    lw t1, 0(t0)
    li t0, 5
    bne s1, t0, fail

    lui t0, 0x90002
    li t1, 80
    sw t1, 4(t0)          /* COMPARE */
    li t1, 3
    sw t1, 8(t0)          /* reset and enable */
    li t0, 0x80
    csrs mie, t0
    csrsi mstatus, 8
wait_for_cat:
    beqz s2, wait_for_cat /* The handler sets s2 and returns here. */
    li t1, 1
    j finish

.balign 4
trap_handler:
    csrr t0, mcause
    li t1, 11
    beq t0, t1, handle_ecall
    li t1, 3
    beq t0, t1, handle_break
    li t1, 5
    beq t0, t1, handle_load_error
    li t1, 0x80000007
    bne t0, t1, fail
    lui t0, 0x90002
    sw zero, 4(t0)        /* Clear the timer interrupt level. */
    lui t0, 0x90000
    la t1, message
    lbu t2, 0(t1)
send:
    sb t2, 0(t0)
    addi t1, t1, 1
    lbu t2, 0(t1)
    bnez t2, send
    li s2, 1
    mret

handle_ecall:
    la t1, ecall_site
    j resume
handle_break:
    la t1, break_site
    j resume
handle_load_error:
    csrr t2, mtval
    lui t3, 0x40000
    bne t2, t3, fail
    la t1, bad_load_site
resume:
    csrr t2, mepc
    bne t2, t1, fail
    addi t2, t2, 4
    csrw mepc, t2
    mv s1, t0
    mret

fail:
    li t1, 3
finish:
    la t0, tohost
    sw t1, 0(t0)
    j .

.section .rodata
message:
    .asciz "Meow!\n"
.section .data
.balign 4
.globl tohost
tohost:
    .word 0
