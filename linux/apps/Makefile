
all: simple_load upgrade_firmware

clean:
	rm -rf simple_load upgrade_firmware

simple_load : simple_load.c
	gcc -o $@ $<

upgrade_firmware: upgrade_firmware.c
	gcc -o $@ $<
