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
	//Enables OCP (Open Core Protocol) between PRU and am33xx
	LBCO r0, CONST_PRUCFG, 4, 4
	CLR r0, r0, 4
	SBCO r0, CONST_PRUCFG, 4, 4

	//Configure the block index register
	MOV r0, 0x00000000
	MOV r1, CTBIR_0
	SBBO r0, r1, #0x00, 4

        CLR r30.t15

        //Outputs a 16 bit clock signal, reading the input gpio on the falling edge.
        LOOP1:
                MOV r0, 0
                MOV r4, 0
                MOV r5, 0
                LOOP2:
                        SET r30.t15
                        WAIT r3, 2000
                        CLR r30.t15

			READGPIO GPIO1, 15, r6, r1
                        LSL r1, r1, r0
                        OR r4, r4, r1

			READGPIO GPIO1, 14, r6, r2
                        LSL r2, r2, r0
                        OR r5, r5, r2

                        WAIT r9, 10000
                        ADD r0, r0, 1
                QBNE LOOP2, r0, 18

		SBCO r4, CONST_PRUDRAM, 0, 4
		SBCO r5, CONST_PRUDRAM, 4, 4

                WAIT r9, 360000
        JMP LOOP1

        HALT

