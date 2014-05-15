#Main program compilation
define control
	gcc -c control.c
	gcc control.o -L/user/lib -lm -lprussdrv -lpthread -o control
	rm control.o
endef

#Compilation of asm program which runs on the PRU
define stepper
	pasm -b stepper.asm | grep Error
endef

#Device tree overlay to enable PRU pins, used for stepper STEP signals
define stepper-pru
	dtc -O dtb -o stepper-pru/stepper-pru-00A0.dtbo -b 0 -@ stepper-pru/stepper-pru-00A0.dts
	cp stepper-pru/stepper-pru-00A0.dts /lib/firmware
	cp stepper-pru/stepper-pru-00A0.dtbo /lib/firmware
endef

#Device tree overlay to enable SPI
define spi
	dtc -I dts -O dtb -o spi/SPI0-00A0.dtbo spi/SPI0-00A0.dts
	dtc -I dts -O dtb -o spi/SPI1-00A0.dtbo spi/SPI1-00A0.dts
	cp spi/SPI0-00A0.dts /lib/firmware
	cp spi/SPI0-00A0.dtbo /lib/firmware
	cp spi/SPI1-00A0.dts /lib/firmware
	cp spi/SPI1-00A0.dtbo /lib/firmware
endef

#Device tree overlay to enable all the GPIOs
define gpio-enable
	dtc -O dtb -o gpio-enable/gpio-enable-00A0.dtbo -b 0 -@ gpio-enable/gpio-enable-00A0.dts
	cp gpio-enable/gpio-enable-00A0.dts /lib/firmware
	cp gpio-enable/gpio-enable-00A0.dtbo /lib/firmware
endef

#Script for loosening the stepper motors/setting the directional control valves
define loosen
	gcc -c loosen/loosen.c -o loosen/loosen.o
	gcc loosen/loosen.o -L/user/lib -lm -lprussdrv -lpthread -o loosen/loosen
	cd loosen && pasm -b stepper.asm | grep Error
	rm loosen/loosen.o
endef

#Script for tightening the stepper motors
define tighten
	gcc -c tighten/tighten.c -o tighten/tighten.o
	gcc tighten/tighten.o -L/user/lib -lm -lprussdrv -lpthread -o tighten/tighten
	cd tighten && pasm -b stepper.asm | grep Error
	rm tighten/tighten.o
endef

.PHONY: all control stepper dts stepper-pru spi gpio-enable loosen tighten

all:
	$(control)
	$(stepper)
	$(stepper-pru)
	$(spi)
	$(gpio-enable)
	$(loosen)
	$(tighten)

control: control.c
	$(control)

stepper: stepper.asm
	$(stepper)

dto: stepper-pru/stepper-pru-00A0.dts spi/SPI0-00A0.dts spi/SPI1-00A0.dts gpio-enable/gpio-enable-00A0.dts
	$(stepper-pru)
	$(spi)
	$(gpio-enable)

stepper-pru: stepper-pru/stepper-pru-00A0.dts
	$(stepper-pru)

spi: spi/SPI0-00A0.dts spi/SPI1-00A0.dts
	$(spi)

gpio-enable: gpio-enable/gpio-enable-00A0.dts
	$(gpio-enable)

loosen: loosen/loosen.c loosen/stepper.asm
	$(loosen)

tighten: tighten/tighten.c tighten/stepper.asm
	$(tighten)

