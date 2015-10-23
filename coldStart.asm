.origin 0
.entrypoint START

#define CONST_PRUCFG C4

#define STP_NUM 1000
#define STP_TIME 1000000

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

	CLR r30, r30, 4
	CLR r30, r30, 5

	SET r30, r30, 2
	SET r30, r30, 3

	MOV r0, 0
	MOV r1, STP_NUM
LOOP1:
	XOR r30, r30, 3 //Toggles pins 1 and 2
	ADD r0, r0, 1
	WAIT r2, STP_TIME
	QBNE LOOP1, r0, r1

HALT

