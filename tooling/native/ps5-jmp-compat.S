/*
 * Tail-call compatibility symbols for OpenSSL's _setjmp/_longjmp imports.
 *
 * These must be jumps, not C wrappers: setjmp must save the caller's frame,
 * and a wrapper frame would have returned before a later longjmp resumed it.
 * Ordinary relocations to the SDK imports are resolved with the weak stubs.
 */
    .text
    .p2align 4
    .globl _setjmp
    .hidden _setjmp
    .type _setjmp,@function
_setjmp:
    jmp setjmp
    .size _setjmp,.-_setjmp

    .p2align 4
    .globl _longjmp
    .hidden _longjmp
    .type _longjmp,@function
_longjmp:
    jmp __longjmp
    .size _longjmp,.-_longjmp
