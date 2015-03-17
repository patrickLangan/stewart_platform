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
#define CONTROL_DIR	508

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

.macro WIPERAM
.mparam offset, reg, end
	MOV reg, 0
LOOP1:
        SBCO reg, CONST_PRUDRAM, offset, 4
	ADD offset, offset, 4
	QBLT LOOP1, end, offset
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
        WRITERAM r0, r1, 1 << 0
        WRITERAM r0, r1, 1 << 1
        WRITERAM r0, r1, 1 << 2
        WRITERAM r0, r1, 1 << 3
        WRITERAM r0, r1, 1 << 4
        WRITERAM r0, r1, 1 << 5
        WRITERAM r0, r1, 1 << 6
        WRITERAM r0, r1, 1 << 7
        WRITERAM r0, r1, 1 << 8
        WRITERAM r0, r1, 1 << 9
        WRITERAM r0, r1, 1 << 10
        WRITERAM r0, r1, 1 << 12
        //Motor DIR
        WRITERAM r0, r1, 1 << 31
        WRITERAM r0, r1, 1 << 1
        WRITERAM r0, r1, 1 << 2
        WRITERAM r0, r1, 1 << 3
        WRITERAM r0, r1, 1 << 4
        WRITERAM r0, r1, 1 << 5
        WRITERAM r0, r1, 1 << 6
        WRITERAM r0, r1, 1 << 7
        WRITERAM r0, r1, 1 << 16
        WRITERAM r0, r1, 1 << 13
        WRITERAM r0, r1, 1 << 14
        WRITERAM r0, r1, 1 << 15
        //Control Valve
        WRITERAM r0, r1, 1 << 1
        WRITERAM r0, r1, 1 << 2
        WRITERAM r0, r1, 1 << 3
        WRITERAM r0, r1, 1 << 4
        WRITERAM r0, r1, 1 << 5
        WRITERAM r0, r1, 1 << 12
        //Rotary Encoder 1
        WRITERAM r0, r1, 7
        WRITERAM r0, r1, 30
        WRITERAM r0, r1, 9
        WRITERAM r0, r1, 8
        WRITERAM r0, r1, 11
        WRITERAM r0, r1, 13
        WRITERAM r0, r1, 14
        WRITERAM r0, r1, 15
        WRITERAM r0, r1, 20
        WRITERAM r0, r1, 22
        WRITERAM r0, r1, 23
        WRITERAM r0, r1, 26
        //Rotary Encoder 2
        WRITERAM r0, r1, 12
        WRITERAM r0, r1, 17
        WRITERAM r0, r1, 18
        WRITERAM r0, r1, 19
        WRITERAM r0, r1, 28
        WRITERAM r0, r1, 29
        WRITERAM r0, r1, 0
        WRITERAM r0, r1, 14
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

	MOV r2, 532
	WIPERAM r0, r1, r2


	//This loop goes through rotary encoders channel 1, setting the initial values
	MOV r0, LAST_ENCODER1
	MOV r4, LAST_ENCODER2
INIT1:
	SUB r1, r0, 240
	READRAM r1, 0, r2, r3
	READGPIO GPIO0, r3, r2, r1
	WRITERAM r0, r2, r1
	QBNE INIT1, r0, r4

	//This loop goes through rotary encoders channel 2, setting the initial values
	MOV r4, LAST_ENCODER2
	add r4, r4, 48
	MOV r5, 488
INIT2:
	SUB r1, r0, 240
	READRAM r1, 0, r2, r3
	QBGT BUS1i, r0, r5
	READGPIO GPIO2, r3, r2, r1
	JMP DONEGPIOi
BUS1i:
	READGPIO GPIO1, r3, r2, r1
DONEGPIOi:
	WRITERAM r0, r2, r1
	QBNE INIT2, r0, r4


	//MAIN PROGRAM LOOP
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

        READRAM 0, r0, r1, r2

	//Load encoder position
	//Set to zero if negitive (valve should never go to a negitive position anyway)
	//Divide position by 2 (to match the stepper resolution, problems occur otherwise)
        READRAM STEP_POSITION, r0, r1, r3
        QBBC POSPOS, r3, 31
        MOV r3, 0
POSPOS:
	LSR r3, r3, 1

	//Load control valve directions, bitshifting to match the control valve to stepper number
	LSR r8, r0, 3
	LSL r8, r8, 2
        READRAM CONTROL_DIR, r8, r4, r5

	//Set the goal position to zero if a direction change is required
	//This results in both motors in a pair to go to zero before the control valve switches
        QBBS NEGSETPOS, r2, 31
        QBNE POSSETPOS, r5, 0
        MOV r2, 0
        JMP POSSETPOS
NEGSETPOS:
        //Convert negitive number to positive
        SUB r2, r2, 1
        NOT r2, r2
        QBNE POSSETPOS, r5, 1
        MOV r2, 0
POSSETPOS:

        QBEQ NXTMOT, r2, r3

	//Set the direction of the motor
        QBLT DIRDOWN, r3, r2
        MOV r2, GPIO1 | GPIO_CLEARDATAOUT
        JMP DIRUP
DIRDOWN:
        MOV r2, GPIO1 | GPIO_SETDATAOUT
DIRUP:
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


	//This loop goes through rotary encoders and updates their positions as nesecary
	MOV r0, 0 //Index
LOOP3:
	//Read the current encoder values
	//Channel 2 is split up amongst 2 internal bus's, hence the branching
	READRAM ROTARY_ENCODER1, r0, r2, r1
	READGPIO GPIO0, r1, r3, r2
	READRAM ROTARY_ENCODER2, r0, r3, r1
	QBGT BUS1, r0, 28
	READGPIO GPIO2, r1, r4, r3
	JMP DONEGPIO
BUS1:
	READGPIO GPIO1, r1, r4, r3
DONEGPIO:

	//Read the stored encoder position
	READRAM STEP_POSITION, r0, r6, r7

	//Look for rising or falling edges in each channel
	//Whether to inc or dec depends the value of the other channel
	READRAM LAST_ENCODER1, r0, r4, r1
	QBEQ NXTCHAN, r2, r1
	QBEQ INC, r2, r3
	ADD r7, r7, 1
	JMP NXTCHAN
INC:
	SUB r7, r7, 1
NXTCHAN:
	READRAM LAST_ENCODER2, r0, r5, r1
	QBEQ OUT, r3, r1
	QBEQ DEC, r3, r2
	SUB r7, r7, 1
	JMP OUT
DEC:
	ADD r7, r7, 1

OUT:
	//Store the encoder values and update the position
        SBCO r2, CONST_PRUDRAM, r4, 4 //Last Encoder 1
        SBCO r3, CONST_PRUDRAM, r5, 4 //Last Encoder 2
        SBCO r7, CONST_PRUDRAM, r6, 4 //Step Position

	//Increment index and move onto the next encoder
	ADD r0, r0, 4
	QBNE LOOP3, r0, 48


        //This loop changes the directions of the control valves if nessecary
        MOV r0, 0 //Index
        MOV r1, 0 //Index2
LOOP4:
	//Load the encoder position of the first motor
	//Set to zero if negitive (valve should never go to a negitive position anyway)
	//Divide position by 2 (to match the stepper resolution, problems occur otherwise)
        READRAM STEP_POSITION, r0, r3, r2
        QBBC POSPOS1, r2, 31
	MOV r2, 0
POSPOS1:
	LSR r2, r2, 1

	//If the first motor's position isn't zero, don't switch the control valve
        ADD r0, r0, 4
        QBNE NOSWITCH, r2, 0

	//Read second motor encoder as done for the first motor
        READRAM STEP_POSITION, r0, r3, r2
        QBBC POSPOS2, r2, 31
	MOV r2, 0
POSPOS2:
	LSR r2, r2, 1

	//If the second motor's position isn't zero, don't switch the control valve
        QBNE NOSWITCH, r2, 0

	//Read set position for the second motor in the pair (should both have the same set position for now)
        READRAM 0, r0, r3, r2
	//Don't switch the control valve if the set position is zero (no need, valves are closed)
        QBEQ NOSWITCH, r2, 0

	//Switch the control valve in the correct direction
        READRAM CONTROL_DIR, r1, r4, r3
        QBBS NEGATIVE, r2, 31
        MOV r5, 1
	QBEQ BUS0NEG, r1, 20
        MOV r2, GPIO2 | GPIO_SETDATAOUT
	JMP STOREDIR
BUS0NEG:
        MOV r2, GPIO0 | GPIO_SETDATAOUT
        JMP STOREDIR
NEGATIVE:
        MOV r5, 0
	QBEQ BUS0POS, r1, 20
        MOV r2, GPIO2 | GPIO_CLEARDATAOUT
	JMP STOREDIR
BUS0POS:
        MOV r2, GPIO0 | GPIO_CLEARDATAOUT

STOREDIR:
        SBCO r5, CONST_PRUDRAM, r4, 4 //Control Dir
        READRAM CONTROL_VALVE, r1, r3, r4
        SBBO r4, r2, 0, 4

NOSWITCH:
        ADD r0, r0, 4
        ADD r1, r1, 4
        QBNE LOOP4, r0, 48

	JMP LOOP1

HALT

