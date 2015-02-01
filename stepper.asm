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
	MOV r1, 0

	//Set the direction of the control valve down
	MOV r2, 0

LOOP1:
	//Get desired step position of motor pair 1
	LBCO r3, CONST_PRUDRAM, 0, 4

	//Test if the desired position is positive or negitive
	MOV r4, 1 << 31
	AND r4, r3, r4
	QBEQ POS1, r4, 0

	//Convert negitive number to positive
	SUB r3, r3, 1
	NOT r3, r3

	//If the valves are closed (r1 = 0), there may be a direction change
	QBNE DIRSET, r1, 0

	//If there is a change in direction, flip the control valve
	QBEQ DIRSET, r2, 1
	MOV r6, VALVE1;
	MOV r4, GPIO2 | GPIO_SETDATAOUT
	SBBO r6, r4, 0, 4
	MOV r2, 1

	JMP DIRSET

POS1:
	//If the valves are closed (r1 = 0), there may be a direction change
	QBNE DIRSET, r1, 0

	//If there is a change in direction, flip the control valve
	QBEQ DIRSET, r2, 0
	MOV r6, VALVE1;
	MOV r4, GPIO2 | GPIO_CLEARDATAOUT
	SBBO r6, r4, 0, 4
	MOV r2, 0

DIRSET:
	//If motor 1 is already where it needs to be, jump back to the loop start and check again
	QBEQ LOOP1, r1, r3

	//If motor 1 is less than the desired position, jump to DIRUP1, otherwise set DIR down and add a step
	QBLE DIRUP1, r1, r3

	//Set DIR down
	MOV r3, DIR1;
	MOV r4, GPIO1 | GPIO_CLEARDATAOUT
	SBBO r3, r4, 0, 4

	//Add a step
	ADD r1, r1, 1

	JMP STEP1

DIRUP1:
	//Set DIR up
	MOV r3, DIR1;
	MOV r4, GPIO1 | GPIO_SETDATAOUT
	SBBO r3, r4, 0, 4

	//Subtract a step
	SUB r1, r1, 1

STEP1:
	//Move the motor pair one step by toggling their STEP pin
	MOV r3, STP1
	XOR r30, r30, r3

	//Wait an arbitrary amount of time between steps
	WAIT r3, 50000
	JMP LOOP1

HALT

