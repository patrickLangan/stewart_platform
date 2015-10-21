.origin 0
.entrypoint START

#define CONST_PRUCFG C4

#define CONST_PRUDRAM C24
#define CTBIR_1 0x22024

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

	//Configure the block index register
	MOV r0, 0x00000000
	MOV r1, CTBIR_1
	SBBO r0, r1, #0x00, 4

	//Make sure steppers are zeroed and control valve is closed before starting

	MOV r0, 0
	MOV r1, 0
	MOV r2, 1
LOOP1:
	LBCO r3, CONST_PRUDRAM, 0, 4
	LBCO r4, CONST_PRUDRAM, 4, 4

	MAX r5, r3, r4
	MOV r6, 1
	QBEQ IN, r5, 0
	QBBS NEG, r5, 31
	QBEQ OUT, r2, 2
	MOV r6, 2
	JMP IN
NEG:
	SUB r3, r3, 1
        NOT r3, r3
	SUB r4, r4, 1
        NOT r4, r4
	QBEQ OUT, r2, 0
	MOV r6, 0
IN:
	MOV r3, 0
	MOV r4, 0
	OR r7, r0, r1
	QBNE OUT, r7, 0

	MOV r2, r6
	QBEQ EXTEND, r6, 2
	QBEQ RTRACT, r6, 0
	CLR r30, r30, 4
	CLR r30, r30, 5
	JMP OUT
EXTEND:
	SET r30, r30, 4
	CLR r30, r30, 5
	JMP OUT
RTRACT:
	CLR r30, r30, 4
	SET r30, r30, 5
OUT:

	QBEQ NXT_MOT, r0, r3
	QBGT POS_DIR, r0, r3
	CLR r30, r30, 2
	SUB r0, r0, 1
	JMP STEP
POS_DIR:
	SET r30, r30, 2
	ADD r0, r0, 1
STEP:
	XOR r30, r30, 1 << 0

NXT_MOT:
	QBEQ OUT2, r1, r4
	QBGT POS_DR2, r1, r4
	CLR r30, r30, 3
	SUB r1, r1, 1
	JMP STP2
POS_DR2:
	SET r30, r30, 3
	ADD r1, r1, 1
STP2:
	XOR r30, r30, 1 << 1
OUT2:

	WAIT r3, STP_TIME
	JMP LOOP1

HALT

