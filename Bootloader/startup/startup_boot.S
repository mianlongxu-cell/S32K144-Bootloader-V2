    .syntax unified
    .arch armv7e-m
    .thumb

    .section .isr_vector, "a", %progbits
    .align 7
    .global __boot_vector_table
__boot_vector_table:
    .word __StackTop
    .word Reset_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word Default_Handler
    .word Default_Handler

    .section .FlashConfig, "a", %progbits
    .word 0xFFFFFFFF
    .word 0xFFFFFFFF
    .word 0xFFFFFFFF
    .word 0xFFFF7FFE

    .section .text.Reset_Handler, "ax", %progbits
    .align 2
    .thumb_func
    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    cpsid i

    /* S32K144 SRAM is ECC protected.  Initialize the complete 64 KB SRAM
     * before the first stack access, matching the original NXP startup. */
    ldr r0, =0x1FFF8000
    /* Keep the final 16 bytes for the one-shot software boot request. */
    ldr r1, =0x20006FF0
    movs r2, #0
0:
    str r2, [r0], #4
    cmp r0, r1
    bcc 0b

    ldr sp, =__StackTop

    ldr r0, =__DATA_ROM
    ldr r1, =__DATA_RAM
    ldr r2, =__DATA_END
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b

2:
    ldr r0, =__BSS_START
    ldr r1, =__BSS_END
    movs r2, #0
3:
    cmp r0, r1
    bcs 4f
    str r2, [r0], #4
    b 3b

4:
    bl main
5:
    b 5b

    .size Reset_Handler, . - Reset_Handler

    .section .text.Default_Handler, "ax", %progbits
    .align 2
    .thumb_func
    .global Default_Handler
Default_Handler:
    b Default_Handler
