/*
 * setjmp_aarch64.S — setjmp/longjmp for AArch64
 *
 * jmp_buf layout (192 bytes):
 *   [0..72]   x19-x29 (callee-saved regs, 11 regs × 8 bytes = 88)
 *   [88]      x30 (lr, return address)
 *   [96]      sp  (stack pointer)
 *   [104..232] d8-d15 (callee-saved FP regs, 8 regs × 8 bytes = 64)
 */

.global setjmp
.global _setjmp
.global longjmp
.global _longjmp

.text

setjmp:
_setjmp:
    /* Save callee-saved registers x19-x29 */
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    str x29,       [x0, #80]

    /* Save lr (return address) and sp */
    mov x2, sp
    stp x30, x2,  [x0, #88]

    /* Save callee-saved FP registers d8-d15 */
    stp d8,  d9,  [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]

    /* Return 0 */
    mov w0, #0
    ret

longjmp:
_longjmp:
    /* Restore callee-saved registers */
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldr x29,       [x0, #80]

    /* Restore lr and sp */
    ldp x30, x2,  [x0, #88]
    mov sp, x2

    /* Restore FP registers */
    ldp d8,  d9,  [x0, #104]
    ldp d10, d11, [x0, #120]
    ldp d12, d13, [x0, #136]
    ldp d14, d15, [x0, #152]

    /* Return val (or 1 if val == 0) */
    mov w0, w1
    cmp w0, #0
    cinc w0, w0, eq
    ret
