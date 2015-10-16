All:
	gcc -c control.c
	gcc control.o -L/user/lib -lm -lprussdrv -lpthread i2cfunc.c -o control
	rm control.o
	pasm -b length.asm | grep Error

