All:
	gcc -c coldStart.c
	gcc coldStart.o -L/user/lib -lprussdrv -o coldStart
	rm coldStart.o
	gcc -c test.c
	gcc test.o -L/user/lib -lm -lprussdrv -lpthread i2cfunc.c -o test
	rm test.o
	gcc -c control.c
	gcc control.o -L/user/lib -lm -lprussdrv -lpthread i2cfunc.c -o control
	rm control.o
	pasm -b length.asm | grep Error
	pasm -b stepper.asm | grep Error
	pasm -b coldStart.asm | grep Error

