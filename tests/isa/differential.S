/* Same ELF for Caitsidhe-VS and the independent RV32 emulator. */
.section .text
.globl _start
_start:
    la s0, scratch
    li s1, 64
    li s2, 0x800007ff
    li s3, -1
    addi zero, s2, 1
round:
    add a0, s2, s3
    sub a1, s2, s3
    xor a2, a0, a1
    or a3, s2, a2
    and a4, a3, s3
    sll a5, a4, s1
    srl a6, a5, s1
    sra a7, s2, s1
    slt t0, a7, a6
    sltu t1, a7, a6
    addi t2, a7, -2048
    xori t3, t2, -1
    ori t4, t3, 2047
    andi t5, t4, -2048
    slli t6, t5, 31
    srli t6, t6, 13
    srai t5, t5, 31
    slti t0, t5, -1
    sltiu t1, t5, -1

    sw a0, 0(s0)
    sh t3, 2(s0)
    sb s1, 1(s0)
    fence rw, rw
    lw a0, 0(s0)
    add s2, a0, s2
    lb a1, 3(s0)
    lbu a2, 3(s0)
    lh a3, 2(s0)
    lhu a4, 2(s0)
    lb zero, 0(s0)
    bgeu a2, a1, 1f
    xori s2, s2, 5
1:
    blt a3, a4, 2f
    xori s2, s2, 7
2:
    bge a3, a4, 3f
    xori s2, s2, 11
3:
    bltu a1, a2, 4f
    xori s2, s2, 13
4:
    beq a1, a2, 5f
    xori s2, s2, 17
5:
    addi s1, s1, -1
    bne s1, zero, round
    jal ra, call
    j done
call:
    addi t1, ra, 1
    jalr t2, 0(t1)
done:
    ebreak

.section .bss
.balign 4
scratch:
    .space 4
