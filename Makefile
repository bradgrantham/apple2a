CC65    =       $(HOME)/trees/cc65/bin/
CPU     =       6502

apple2a.rom: a.out
	(dd count=5 bs=4096 if=/dev/zero ; cat a.out) > apple2a.rom


a.out: main.o interrupt.o vectors.o apple2rom.cfg apple2rom.lib
	$(CC65)/ld65 -C apple2rom.cfg -m main.map --dbgfile main.dbg interrupt.o vectors.o main.o apple2rom.lib

clean:
	rm *.o a.out main.s

main.s: main.c
	$(CC65)/cc65 -t none -O --cpu $(CPU) main.c

main.o: main.s
	$(CC65)/ca65 --cpu $(CPU) main.s

interrupt.o: interrupt.s
	$(CC65)/ca65 --cpu $(CPU) interrupt.s

vectors.o: vectors.s
	$(CC65)/ca65 --cpu $(CPU) vectors.s

crt0.o: crt0.s
	$(CC65)/ca65 --cpu $(CPU) crt0.s

apple2rom.lib: crt0.o supervision.lib
	cp supervision.lib apple2rom.lib
	../cc65/bin/ar65 a apple2rom.lib crt0.o
