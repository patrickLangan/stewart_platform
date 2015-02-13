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
#define CTBIR_1 0x22424

.macro WRITERAM
.mparam addr, gpio
        MOV r1, gpio
        SBCO r1, CONST_PRUDRAM, addr, 4
        ADD addr, addr, 4
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
	MOV r1, CTBIR_1
	SBBO r0, r1, #0x00, 4

	//Load GPIO/PRU addresses into RAM
        MOV r0, 28
	//Motor STEP
	WRITERAM r0, 0b00000000000001	//28
	WRITERAM r0, 0b00000000000010
	WRITERAM r0, 0b00000000000100
	WRITERAM r0, 0b00000000001000
	WRITERAM r0, 0b00000000010000
	WRITERAM r0, 0b00000000100000
	WRITERAM r0, 0b00000001000000
	WRITERAM r0, 0b00000010000000
	WRITERAM r0, 0b00000100000000
	WRITERAM r0, 0b00001000000000
	WRITERAM r0, 0b00100000000000
	WRITERAM r0, 0b01000000000000
	//Motor DIR
	WRITERAM r0, 0b0000000000000001 //76
	WRITERAM r0, 0b0000000000000010
	WRITERAM r0, 0b0000000000000100
	WRITERAM r0, 0b0000000000001000
	WRITERAM r0, 0b0000000000010000
	WRITERAM r0, 0b0000000000100000
	WRITERAM r0, 0b0000000001000000
	WRITERAM r0, 0b0000000010000000
	WRITERAM r0, 0b0001000000000000
	WRITERAM r0, 0b0010000000000000
	WRITERAM r0, 0b0100000000000000
	WRITERAM r0, 0b1000000000000000
	//Control Valve
	WRITERAM r0, 0b000000000000010 //124
	WRITERAM r0, 0b000000000000100
	WRITERAM r0, 0b000000000001000
	WRITERAM r0, 0b000000000010000
	WRITERAM r0, 0b000000000100000
	WRITERAM r0, 0b100000000000000
	//Home Switch
	WRITERAM r0, 0b000000000000000000000100000000 //148
	WRITERAM r0, 0b000000000000000000001000000000
	WRITERAM r0, 0b000000000000000000010000000000
	WRITERAM r0, 0b000000000000000000100000000000
	WRITERAM r0, 0b000000010000000000000000000000
	WRITERAM r0, 0b000000100000000000000000000000
	WRITERAM r0, 0b000000000000010000000000000000
	WRITERAM r0, 0b000000000000100000000000000000
	WRITERAM r0, 0b000000000001000000000000000000
	WRITERAM r0, 0b000000000010000000000000000000
	WRITERAM r0, 0b010000000000000000000000000000
	WRITERAM r0, 0b100000000000000000000000000000

	//Initialize variables
	//Motor Pair Position
	WRITERAM r0, 0 //196
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	//Last Home Switch
	WRITERAM r0, 1 //220
	//Rotation Number
	WRITERAM r0, 0 //224
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0
	WRITERAM r0, 0

	//Set the initial motor positions and store the initial home switch values
	MOV r1, GPIO0 | GPIO_DATAIN
        LBBO r0, r1, 0, 4
        SBCO r0, CONST_PRUDRAM, 220, 4 //Last Home Switch
        LBCO r1, CONST_PRUDRAM, 148, 4 //Home Switch
        AND r1, r1, r0
        QBNE NXTINIT, r1, 0 //If the home switch reads zero the valve isn't closed
        SBCO 1, CONST_PRUDRAM, 224, 4 //Rotation Number
NXTINIT:
        LBCO r1, CONST_PRUDRAM, 152, 4 //Home Switch
        AND r1, r1, r0
        QBNE LOOP1, r1, 0 //If the home switch reads zero the valve isn't closed
        SBCO 1, CONST_PRUDRAM, 228, 4 //Rotation Number

LOOP1:
	//Get desired step position of motor pair 1
	LBCO r1, CONST_PRUDRAM, 0, 4

	//If the motor pair isn't yet where it needs to be, jump to MOVE
        LBCO r0, CONST_PRUDRAM, 196, 4 //Motor Pair Position
	QBNE MOVE, r0, r1

	//If the motor pair should be at zero, make sure it is and fix any offending motors
	QBNE LOOP1, r1, 0
	//Set the motor directions to close the valves
	MOV r2, GPIO1 | GPIO_SETDATAOUT
        LBCO r1, CONST_PRUDRAM, 76, 4 //Motor DIR
	SBBO r1, r2, 0, 4
	MOV r7, 0 //Store step direction
	WAIT r0, 70000
	LBCO r1, CONST_PRUDRAM, 224, 4 //Rotation Number
	QBEQ NXTHOME, r1, 0 //If rotation number isn't zero step the motor
        LBCO r1, CONST_PRUDRAM, 28, 4 //Motor STEP
	XOR r30, r30, r1
NXTHOME:
	LBCO r1, CONST_PRUDRAM, 228, 4 //Rotation Number
	QBEQ STEPWAIT, r1, 0 //If rotation number isn't zero step the motor
        LBCO r1, CONST_PRUDRAM, 32, 4 //Motor STEP
	XOR r30, r30, r1
STEPWAIT:
	WAIT r0, 70000
	JMP CHKHOME

MOVE:
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
        LBCO r3, CONST_PRUDRAM, 124, 4 //Control Valve
	SBBO r3, r4, 0, 4

	//Set the motor direction
	QBLT DIRUP, r0, r1
	MOV r2, GPIO1 | GPIO_CLEARDATAOUT
	ADD r0, r0, 1
	MOV r7, 1 //Store step direction
	JMP SETDIR
DIRUP:
	MOV r2, GPIO1 | GPIO_SETDATAOUT
	SUB r0, r0, 1
	MOV r7, 0 //Store step direction
SETDIR:
        SBCO r0, CONST_PRUDRAM, 196, 4 //Motor Pair Position
        LBCO r1, CONST_PRUDRAM, 76, 4 //Motor DIR
	SBBO r1, r2, 0, 4

	WAIT r1, 70000 //Wait an arbitrary amount of time between steps

	//Move both motors of the pair one step by toggling their STEP pins
        LBCO r1, CONST_PRUDRAM, 28, 4 //Motor STEP
	XOR r30, r30, r1
        LBCO r1, CONST_PRUDRAM, 32, 4 //Motor STEP
	XOR r30, r30, r1

	WAIT r1, 70000 //Wait an arbitrary amount of time between steps

CHKHOME:
	//Look at the home switch to see if a rotation has occurred
	MOV r1, GPIO0 | GPIO_DATAIN
        LBBO r0, r1, 0, 4
        LBCO r1, CONST_PRUDRAM, 148, 4 //Home Switch
        LBCO r2, CONST_PRUDRAM, 220, 4 //Last Home Switch
        AND r2, r2, r1
        AND r1, r1, r0
        QBEQ NXTROT, r1, r2 //If the current and last home switch readings are different, update the motor rotation
        LBCO r1, CONST_PRUDRAM, 224, 4 //Rotation Number
	QBEQ NEG1, r7, 0
	ADD r1, r1, 1
	JMP POS1
NEG1:
	SUB r1, r1, 1
POS1:
        SBCO r1, CONST_PRUDRAM, 224, 4 //Rotation Number
NXTROT:
        LBCO r1, CONST_PRUDRAM, 152, 4 //Home Switch
        LBCO r2, CONST_PRUDRAM, 220, 4 //Last Home Switch
        AND r2, r2, r1
        AND r1, r1, r0
        QBEQ STRLAST, r1, r2 //If the current and last home switch readings are different, update the motor rotation
        LBCO r1, CONST_PRUDRAM, 228, 4 //Rotation Number
	QBEQ NEG2, r7, 0
	ADD r1, r1, 1
	JMP POS2
NEG2:
	SUB r1, r1, 1
POS2:
        SBCO r1, CONST_PRUDRAM, 228, 4 //Rotation Number

STRLAST:
        SBCO r0, CONST_PRUDRAM, 220, 4 //Last Home Switch

	JMP LOOP1

HALT

