.origin 0
.entrypoint START

#define CONST_PRUCFG C4

#define GPIO0   0x44E07000
#define GPIO1	0x4804C000 
#define GPIO2	0x481AC000 

#define GPIO_DATAIN		0x138
#define GPIO_CLEARDATAOUT	0x190
#define GPIO_SETDATAOUT		0x194

#define CONST_PRUDRAM C24
#define CTBIR_0 0x22020
#define CTBIR_1 0x22024

.macro READGPIO
.mparam gpio, pin, reg, out
	MOV reg, gpio | GPIO_DATAIN
        LBBO out, reg, 0, 4

	QBBS ON, out, pin
	MOV out, 0
	JMP OFF
ON:
	MOV out, 1
OFF:
.endm

.macro WAIT
.mparam reg, clicks
	MOV reg, clicks
LOOP1:
	SUB reg, reg, 1
	QBNE LOOP1, reg, 0
        ADD reg, reg, 1
.endm

START:
	LBCO r0, CONST_PRUCFG, 4, 4
	CLR r0, r0, 4
	SBCO r0, CONST_PRUCFG, 4, 4

	MOV r0, 0x00000000
	MOV r1, CTBIR_0
	SBBO r0, r1, #0x00, 4

	MOV r5, 4095

	/* SPI read code for MCP3201 */
        SET r30.t15 /* Clock */
	SET r30.t14 /* Chip select */
        LOOP1:
                MOV r0, 15 /* number of bits to read */
                MOV r4, 0

                WAIT r9, 63 /* t_CSH */

		CLR r30.t14
		CLR r30.t15
		WAIT r3, 10 /* t_SUCS */

                LOOP2:
                        SET r30.t15

			READGPIO GPIO1, 15, r6, r1
                        SUB r0, r0, 1
                        LSL r1, r1, r0
                        OR r4, r4, r1

                        WAIT r9, 32 /* t_HI */
                        CLR r30.t15
                        WAIT r9, 32 /* t_LO */
                QBNE LOOP2, r0, 0

		/* Remove NULL and undef bits */
		AND r4, r4, r5

		SBCO r4, CONST_PRUDRAM, 0, 4

		SET r30.t14
        JMP LOOP1

        HALT

