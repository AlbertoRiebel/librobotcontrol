;* PWM servo con RPMsg - llama a process_rpmsg() al final de cada período
        .cdecls "main_pru1.c"
        .ref    ||process_rpmsg||

        .clink
        .global start

; Pin definitions
        .asg    r30.t8,     CH1BIT  ; P8_27
        .asg    r30.t10,    CH2BIT  ; P8_28
        .asg    r30.t9,     CH3BIT  ; P8_29
        .asg    r30.t11,    CH4BIT  ; P8_30
        .asg    r30.t6,     CH5BIT  ; P8_39
        .asg    r30.t7,     CH6BIT  ; P8_40
        .asg    r30.t4,     CH7BIT  ; P8_41
        .asg    r30.t5,     CH8BIT  ; P8_42

        .asg    C28,    CONST_PRUSHAREDRAM
        .asg    C4,     CONST_SYSCFG
        .asg    0x24000,    PRU1_CTRL
        .asg    0x28,       CTPPR0
        .asg    0x100,      SHARED_RAM

start:
        ; Configurar C28 para apuntar a shared RAM en 0x10000
        LDI     r0, SHARED_RAM
        LDI32   r1, PRU1_CTRL + CTPPR0
        SBBO    &r0, r1, 0, 4

        LDI     r9, 0x0
        LDI     r0, 0x0
        LDI     r1, 0x0
        LDI     r2, 0x0
        LDI     r3, 0x0
        LDI     r4, 0x0
        LDI     r5, 0x0
        LDI     r6, 0x0
        LDI32   r7, 0x0
        LDI     r30, 0x0

; Loop principal - 48 instrucciones exactas
CH1:
        QBEQ    CLR1, r0, 0
        SET     r30, CH1BIT
        SUB     r0, r0, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 0, 4
CH2:
        QBEQ    CLR2, r1, 0
        SET     r30, CH2BIT
        SUB     r1, r1, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 4, 4
CH3:
        QBEQ    CLR3, r2, 0
        SET     r30, CH3BIT
        SUB     r2, r2, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 8, 4
CH4:
        QBEQ    CLR4, r3, 0
        SET     r30, CH4BIT
        SUB     r3, r3, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 12, 4
CH5:
        QBEQ    CLR5, r4, 0
        SET     r30, CH5BIT
        SUB     r4, r4, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 16, 4
CH6:
        QBEQ    CLR6, r5, 0
        SET     r30, CH6BIT
        SUB     r5, r5, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 20, 4
CH7:
        QBEQ    CLR7, r6, 0
        SET     r30, CH7BIT
        SUB     r6, r6, 1
        CLR     r9, r9.t1
        SBCO    &r9, CONST_PRUSHAREDRAM, 24, 4
CH8:
        QBEQ    CLR8, r7, 0
        SET     r30, CH8BIT
        SUB     r7, r7, 1
        SBCO    &r9, CONST_PRUSHAREDRAM, 28, 4
        QBA     CH1

CLR1:
        CLR     r30, CH1BIT
        LBCO    &r0, CONST_PRUSHAREDRAM, 0, 4
        QBA     CH2
CLR2:
        CLR     r30, CH2BIT
        LBCO    &r1, CONST_PRUSHAREDRAM, 4, 4
        QBA     CH3
CLR3:
        CLR     r30, CH3BIT
        LBCO    &r2, CONST_PRUSHAREDRAM, 8, 4
        QBA     CH4
CLR4:
        CLR     r30, CH4BIT
        LBCO    &r3, CONST_PRUSHAREDRAM, 12, 4
        QBA     CH5
CLR5:
        CLR     r30, CH5BIT
        LBCO    &r4, CONST_PRUSHAREDRAM, 16, 4
        QBA     CH6
CLR6:
        CLR     r30, CH6BIT
        LBCO    &r5, CONST_PRUSHAREDRAM, 20, 4
        QBA     CH7
CLR7:
        CLR     r30, CH7BIT
        LBCO    &r6, CONST_PRUSHAREDRAM, 24, 4
        QBA     CH8
CLR8:
        CLR     r30, CH8BIT
        LBCO    &r7, CONST_PRUSHAREDRAM, 28, 4
        SBCO    &r0, C24, 36, 4
        SBCO    &r1, C24, 40, 4
        SBCO    &r2, C24, 44, 4
        SBCO    &r3, C24, 48, 4
        SBCO    &r4, C24, 52, 4
        SBCO    &r5, C24, 56, 4
        SBCO    &r6, C24, 60, 4
        SBCO    &r7, C24, 64, 4
        SBCO    &r9, C24, 68, 4
        JAL     r29.w2, ||process_rpmsg||
        LBCO    &r0, C24, 36, 4
        LBCO    &r1, C24, 40, 4
        LBCO    &r2, C24, 44, 4
        LBCO    &r3, C24, 48, 4
        LBCO    &r4, C24, 52, 4
        LBCO    &r5, C24, 56, 4
        LBCO    &r6, C24, 60, 4
        LBCO    &r7, C24, 64, 4
        LBCO    &r9, C24, 68, 4
        QBA     CH1
