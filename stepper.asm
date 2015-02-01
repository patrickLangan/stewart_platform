.origin 0
.entrypoint START

#define CONST_PRUCFG c4

#define GPIO1               0x4804c000 
#define GPIO2               0x481AC000 
#define GPIO_CLEARDATAOUT   0x190
#define GPIO_SETDATAOUT     0x194

#define CONST_PRUDRAM C24
#define CTBIR_1 0x22024

#define STP1 0b10000000000000
#define STP2 0b01000000000000

#define DIR1 0b1
#define DIR2 0b10

#define VALVE1 0b10
#define VALVE2 0b10000000000000000000000000

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

	//Zero the step position
	MOV r0, 0

LOOP1:
	//Get desired step position of motor pair 1
	LBCO r1, CONST_PRUDRAM, 0, 4

	//If the motor pair is already where it needs to be, loop until it isn't
	QBEQ LOOP1, r0, r1

	//Get the desired control valve direction
	LBCO r2, CONST_PRUDRAM, 24, 4
	AND r2, r2, 1 << 0

	//Set the control valve direction
	QBEQ CLRDATA, r2, 0
	MOV r4, GPIO2 | GPIO_SETDATAOUT
	JMP SETGPIO
CLRDATA:
	MOV r4, GPIO2 | GPIO_CLEARDATAOUT
SETGPIO:
	MOV r3, VALVE1;
	SBBO r3, r4, 0, 4

	//Set the motor direction
	QBLT DIRUP, r0, r1
	MOV r2, GPIO1 | GPIO_CLEARDATAOUT
	ADD r0, r0, 1
	JMP SETDIR
DIRUP:
	MOV r2, GPIO1 | GPIO_SETDATAOUT
	SUB r0, r0, 1
SETDIR:
	MOV r1, DIR1;
	SBBO r1, r2, 0, 4

	//Wait an arbitrary amount of time between steps (half before and half after stepping)
	WAIT r1, 25000

	//Move the motor pair one step by toggling their STEP pin
	MOV r1, STP1
	XOR r30, r30, r1

	//Second half of the wait time between steps
	WAIT r1, 25000
	JMP LOOP1

HALT

