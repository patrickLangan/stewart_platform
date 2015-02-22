All:
	gcc -c control.c -o control.o
	gcc control.o -L/user/lib -lm -lprussdrv -lpthread -o control
	pasm -b stepper.asm | grep Error
	rm control.o

