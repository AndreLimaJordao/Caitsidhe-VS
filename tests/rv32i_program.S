/* An independent compiler-produced RV32I binary regression.
 * Success executes EBREAK; failure loops until the cycle limit. */
.section .text
.globl _start
_start:
  li t0, 7
  li t1, 5
  add t2, t0, t1
  li t3, 12
  bne t2, t3, fail
  sub t2, t2, t1
  bne t2, t0, fail
  lui a0, 0x80000
  sw t2, 256(a0)
  lw a1, 256(a0)
  bne a1, t0, fail
  li a2, -1
  srai a3, a2, 7
  bne a3, a2, fail
  li a4, 0
  beq a4, zero, pass
fail:
  j fail
pass:
  ebreak
