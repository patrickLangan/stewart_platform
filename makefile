All:
	gcc -c control.c -o control.o
	gcc control.o i2cfunc.c -L/user/lib -lm -lprussdrv -lpthread -o control
	pasm -b stepper.asm | grep Error
	rm control.o

