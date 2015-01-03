All:
	gcc -c control.c -o control.o
	gcc i2cfunc.c control.o -L/user/lib -lm -lprussdrv -lpthread -o control
	pasm -b sensor.asm | grep Error
	rm *~ && rm control.o
