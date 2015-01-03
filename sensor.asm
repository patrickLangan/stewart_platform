.origin 0
.entrypoint START

#define CONST_PRUCFG            c4
#define CTPPR_0                 0x22028
#define CONST_PRUSHAREDRAM      c28
#define GPIO1                   0x4804c000
#define GPIO_0E                 0x134
#define GPIO_DATAIN             0x138
#define PRU0_ARM_INTERRUPT      19

.macro WA1
.mparam reg,clicks
        MOV reg, clicks
        LOOP:
                SUB reg, reg, 1
                QBNE LOOP, reg, 0
        ADD reg, reg, 1
.endm

START:
        // Enable the OCPs
        LBCO r0, CONST_PRUCFG, 4, 4
        CLR r0, r0, 4
        SBCO r0, CONST_PRUCFG, 4, 4

        //Enable shared ram
        MOV r0, 0x00000120
        MOV r1, CTPPR_0
        SBBO r0, r1, #0x00, 2

        //Enable gpio
        MOV r3, GPIO1 | GPIO_0E
        LBBO r2, r3, 0, 4
        SET r2, 12
        LBBO r2, r3, 0, 4
        SET r2, 14
        LBBO r2, r3, 0, 4
        SET r2, 15
        LBBO r2, r3, 0, 4

        MOV r29, GPIO1 | GPIO_DATAIN

        CLR r30.t15

        //Outputs a 16 bit clock signal, reading the 
        //input gpio on the falling edge.
        LOOP:
                MOV r0, 0
                MOV r4, 0
                MOV r5, 0
                MOV r6, 0
                LOOP2:
                        SET r30.t15
                        WA1 r3, 2000
                        CLR r30.t15

                        LBBO r1, r29, 0, 4
                        LBBO r2, r29, 0, 4
                        LBBO r3, r29, 0, 4

                        LSR r1, r1, 12
                        AND r1, r1, 1
                        LSL r1, r1, r0
                        OR r4, r4, r1

                        LSR r2, r2, 15
                        AND r2, r2, 1
                        LSL r2, r2, r0
                        OR r5, r5, r2

                        LSR r3, r3, 14
                        AND r3, r3, 1
                        LSL r3, r3, r0
                        OR r6, r6, r3

                        WA1 r9, 10000
                        ADD r0, r0, 1
                QBNE LOOP2, r0, 16

                SBCO r4, CONST_PRUSHAREDRAM, 0, 4
                SBCO r5, CONST_PRUSHAREDRAM, 4, 4
                SBCO r6, CONST_PRUSHAREDRAM, 8, 4

                MOV r31.b0, PRU0_ARM_INTERRUPT+16
                WA1 r9, 360000
        JMP LOOP

        HALT
