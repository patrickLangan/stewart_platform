.origin 0
.entrypoint START

#define CONST_PRUCFG c4

.macro WAIT
.mparam reg,clicks
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

	//Switch all the PRU outputs on and off as a test
	LOOP1:
		MOV r30, 0
		WAIT r2, 100000000
		MOV r30, 0b11011111111111
		WAIT r2, 100000000
		JMP LOOP1

HALT

