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
#define CTBIR_1 0x22024

#define FUNCTION_TIME	280
#define LOOP_TIME	606

#define MOTOR_STEP	52
#define MOTOR_DIR	100
#define CONTROL_VALVE	148
#define ROTARY_ENCODER1	172
#define ROTARY_ENCODER2	220
#define TIME_STEP	268
#define MOTOR_TIME	316
#define STEP_POSITION	364
#define LAST_ENCODER1	412
#define LAST_ENCODER2	460

.macro WRITERAM
.mparam addr, reg, value
        MOV reg, value
        SBCO reg, CONST_PRUDRAM, addr, 4
        ADD addr, addr, 4
.endm

.macro READRAM
.mparam addr, offset, reg, out
        MOV reg, addr
	ADD reg, reg, offset
        LBCO out, CONST_PRUDRAM, reg, 4
.endm

.macro READGPIO
.mparam pin, reg, out
	MOV reg, GPIO0 | GPIO_DATAIN
        LBBO out, reg, 0, 4

	QBBS ON, out, pin
	MOV out, 0
	JMP OFF
ON:
	MOV out, 1
OFF:
.endm

.macro WIPERAM
.mparam offset, reg, length
	ADD length, length, offset
	MOV reg, 0
LOOP1:
        SBCO reg, CONST_PRUDRAM, offset, 4
	ADD offset, offset, 4
	QBNE LOOP1, length, offset
.endm

.macro WAIT
.mparam reg, clicks
	MOV reg, clicks
LOOP1:
	SUB reg, reg, 1
	QBNE LOOP1, reg, 0
.endm

.macro TIMEADD
.mparam reg1, reg2, time
	MOV reg1, 316
LOOP1:
        LBCO reg2, CONST_PRUDRAM, reg1, 4 //Motor Time
	ADD reg2, reg2, time
        SBCO reg2, CONST_PRUDRAM, reg1, 4 //Motor Time
	ADD reg1, reg1, 4
	MOV reg2, 364
	QBNE LOOP1, reg1, reg2
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
        MOV r0, MOTOR_STEP
	//Motor STEP
	WRITERAM r0, r1, 0b00000000000001
	WRITERAM r0, r1, 0b00000000000010
	WRITERAM r0, r1, 0b00000000000100
	WRITERAM r0, r1, 0b00000000001000
	WRITERAM r0, r1, 0b00000000010000
	WRITERAM r0, r1, 0b00000000100000
	WRITERAM r0, r1, 0b00000001000000
	WRITERAM r0, r1, 0b00000010000000
	WRITERAM r0, r1, 0b00000100000000
	WRITERAM r0, r1, 0b00001000000000
	WRITERAM r0, r1, 0b00100000000000
	WRITERAM r0, r1, 0b01000000000000
	//Motor DIR
	WRITERAM r0, r1, 0b0000000000000001
	WRITERAM r0, r1, 0b0000000000000010
	WRITERAM r0, r1, 0b0000000000000100
	WRITERAM r0, r1, 0b0000000000001000
	WRITERAM r0, r1, 0b0000000000010000
	WRITERAM r0, r1, 0b0000000000100000
	WRITERAM r0, r1, 0b0000000001000000
	WRITERAM r0, r1, 0b0000000010000000
	WRITERAM r0, r1, 0b0001000000000000
	WRITERAM r0, r1, 0b0010000000000000
	WRITERAM r0, r1, 0b0100000000000000
	WRITERAM r0, r1, 0b1000000000000000
	//Control Valve
	WRITERAM r0, r1, 0b000000000000010
	WRITERAM r0, r1, 0b000000000000100
	WRITERAM r0, r1, 0b000000000001000
	WRITERAM r0, r1, 0b000000000010000
	WRITERAM r0, r1, 0b000000000100000
	WRITERAM r0, r1, 0b100000000000000
	//Rotary Encoder 1
	WRITERAM r0, r1, 7
	WRITERAM r0, r1, 8
	WRITERAM r0, r1, 9
	WRITERAM r0, r1, 10
	WRITERAM r0, r1, 11
	WRITERAM r0, r1, 12
	WRITERAM r0, r1, 13
	WRITERAM r0, r1, 14
	WRITERAM r0, r1, 15
	WRITERAM r0, r1, 20
	WRITERAM r0, r1, 22
	//Rotary Encoder 2
	WRITERAM r0, r1, 23
	WRITERAM r0, r1, 26
	WRITERAM r0, r1, 16
	WRITERAM r0, r1, 17
	WRITERAM r0, r1, 18
	WRITERAM r0, r1, 19
	WRITERAM r0, r1, 28
	WRITERAM r0, r1, 29
	WRITERAM r0, r1, 31
	WRITERAM r0, r1, 15
	WRITERAM r0, r1, 16
	WRITERAM r0, r1, 17
	WRITERAM r0, r1, 25

	//Initialize variables
	//Time Step
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000
	WRITERAM r0, r1, 1000000

	MOV r2, 196
	WIPERAM r0, r1, r2

LOOP1:

	//This loop goes through all 12 motors and steps them asynchronously
	MOV r0, 0 //Index
LOOP2:
	//Check if it's time to step the motor
	READRAM TIME_STEP, r0, r1, r2
	READRAM MOTOR_TIME, r0, r1, r3
	QBLT NXTMOT, r2, r3
	SUB r3, r3, r2
	SBCO r3, CONST_PRUDRAM, r1, 4 //Motor Time

	//Check if the motor needs to be stepped
	READRAM 0, r0, r1, r2
	READRAM STEP_POSITION, r0, r1, r3
        QBEQ NXTMOT, r2, r3

	//Set the direction of the motor
        QBLT DIRDOWN, r3, r2
        MOV r2, GPIO1 | GPIO_CLEARDATAOUT
        ADD r3, r3, 1
        JMP DIRUP
DIRDOWN:
        MOV r2, GPIO1 | GPIO_SETDATAOUT
        SUB r3, r3, 1
DIRUP:
        //SBCO r3, CONST_PRUDRAM, r1, 4 //Step Position
	READRAM MOTOR_DIR, r0, r3, r1
        SBBO r1, r2, 0, 4

	//Step the motor
	READRAM MOTOR_STEP, r0, r1, r2
	XOR r30, r30, r2

	//Add the time spent stepping the motor
	MOV r3, FUNCTION_TIME
	TIMEADD r1, r2, r3

NXTMOT:
	//Add the time spent in this loop
	MOV r3, LOOP_TIME
	TIMEADD r1, r2, r3

	//Increment index and move onto the next motor
	ADD r0, r0, 4
	QBNE LOOP2, r0, 48

	//This loop goes through six of the rotary encoders and updates their positions as nesecary
	MOV r0, 0 //Index
LOOP3:
	//Load the previous encoder values and position
	READRAM ROTARY_ENCODER1, r0, r2, r1
	READGPIO r1, r3, r2
	READRAM ROTARY_ENCODER2, r0, r3, r1
	READGPIO r1, r4, r3
	READRAM STEP_POSITION, r0, r6, r7

	//Look for rising or falling edges in each channel.
	//Whether to inc or dec depends the value of the other channel.
	READRAM LAST_ENCODER1, r0, r4, r1
	QBEQ NXTCHAN, r2, r1
	QBEQ INC, r2, r3
	SUB r7, r7, 1
	JMP NXTCHAN
INC:
	ADD r7, r7, 1
NXTCHAN:
	READRAM LAST_ENCODER2, r0, r5, r1
	QBEQ OUT, r3, r1
	QBEQ DEC, r3, r2
	ADD r7, r7, 1
	JMP OUT
DEC:
	SUB r7, r7, 1

OUT:
	//Store the encoder values and update the position
        SBCO r2, CONST_PRUDRAM, r4, 4 //Last Encoder 1
        SBCO r3, CONST_PRUDRAM, r5, 4 //Last Encoder 2
        SBCO r7, CONST_PRUDRAM, r6, 4 //Step Position

	//Increment index and move onto the next encoder
	ADD r0, r0, 4
	QBNE LOOP3, r0, 48

	JMP LOOP1

HALT

