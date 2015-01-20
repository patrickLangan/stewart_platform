.origin 0
.entrypoint START

#define CONST_PRUCFG c4

#define GPIO1               0x4804c000 
#define GPIO_CLEARDATAOUT   0x190
#define GPIO_SETDATAOUT     0x194

#define CONST_PRUDRAM C24
#define CTBIR_0 0x22020
#define CTBIR_1 0x22024

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

	//Copies data in PRU RAM from c-side input to c-side output
	LBCO r3, CONST_PRUDRAM, 4, 4
	SBCO r3, CONST_PRUDRAM, 8, 4

	//Constants obtained from: https://groups.google.com/forum/?fromgroups=#!topic/beagleboard/35ZXP82EQjA%5B1-25-false%5D
	MOV r2, 0b11111111111111111111111111111111
	MOV r3, GPIO1 | GPIO_SETDATAOUT
	MOV r4, GPIO1 | GPIO_CLEARDATAOUT

	//Switch all the PRU and GPIO outputs on and off as a test
	LOOP1:
		SBBO r2, r3, 0, 4
		MOV r30, 0b11011111111111
		WAIT r5, 60000000

		SBBO r2, r4, 0, 4
		MOV r30, 0
		WAIT r5, 60000000

		JMP LOOP1

HALT

