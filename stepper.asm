.origin 0
.entrypoint START

#define CONST_PRUCFG c4

#define GPIO1               0x4804c000 
#define GPIO_CLEARDATAOUT   0x190
#define GPIO_SETDATAOUT     0x194

#define CONST_PRUDRAM C24
#define CTBIR_0 0x22020
#define CTBIR_1 0x22024

#define STP1 0b10000000000000
#define STP2 0b01000000000000

#define DIR1 0b1
#define DIR2 0b10

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

	//Zero the step positions of both motors
	MOV r1, 0
	MOV r2, 0

	LOOP1:
		//Get desired step position of motor 1
		LBCO r3, CONST_PRUDRAM, 0, 4

		//If motor 1 is already where it needs to be, jump to motor 2
		QBEQ MOTOR2, r1, r3

		//If motor 1 is less than the desired position, jump to DIRUP1, otherwise set DIR down and add a step
		QBLE DIRUP1, r1, r3

		MOV r3, DIR1;
		MOV r4, GPIO1 | GPIO_CLEARDATAOUT
		SBBO r3, r4, 0, 4

		ADD r1, r1, 1

		JMP STEP1

	DIRUP1:
		//Set DIR up
		MOV r3, DIR1;
		MOV r4, GPIO1 | GPIO_SETDATAOUT
		SBBO r3, r4, 0, 4

		SUB r1, r1, 1

	STEP1:
		//Toggle stepper 1 STEP
		MOV r3, STP1
		XOR r30, r30, r3

	MOTOR2:
		LBCO r3, CONST_PRUDRAM, 4, 4

		QBEQ WAITLBL, r2, r3

		QBLE DIRUP2, r2, r3

		MOV r3, DIR2;
		MOV r4, GPIO1 | GPIO_CLEARDATAOUT
		SBBO r3, r4, 0, 4

		ADD r2, r2, 1

		JMP STEP2

	DIRUP2:
		MOV r3, DIR2;
		MOV r4, GPIO1 | GPIO_SETDATAOUT
		SBBO r3, r4, 0, 4

		SUB r2, r2, 1

	STEP2:
		MOV r3, STP2
		XOR r30, r30, r3

	WAITLBL:
		WAIT r5, 1000000
		JMP LOOP1

HALT

