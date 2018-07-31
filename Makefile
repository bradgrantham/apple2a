ifeq ($(USER),lk)
	TREES   ?=       $(HOME)/others
else
	TREES   ?=       $(HOME)/trees
endif

CC65	?=	$(TREES)/cc65/bin
APPLE2E	?=	$(TREES)/apple2e/apple2e

CPU     =       6502
ROM	= 	apple2a.rom
LIB	=	apple2rom.lib

$(ROM): a.out
	(dd count=5 bs=4096 if=/dev/zero 2> /dev/null; cat a.out) > $(ROM)

run: $(ROM)
	$(APPLE2E) $(ROM)

a.out: main.o interrupt.o vectors.o platform.o apple2rom.cfg $(LIB)
	$(CC65)/ld65 -C apple2rom.cfg -m main.map --dbgfile main.dbg interrupt.o vectors.o platform.o main.o $(LIB)

clean:
	rm -f *.o a.out main.s $(LIB) tmp.lib

main.s: main.c
	$(CC65)/cc65 -t none -O --cpu $(CPU) main.c

main.o: main.s
	$(CC65)/ca65 --cpu $(CPU) main.s

# platform.c contains inline assembly and code that must not be optimized
platform.s: platform.c
	$(CC65)/cc65 -t none --cpu $(CPU) platform.c

platform.o: platform.s
	$(CC65)/ca65 --cpu $(CPU) platform.s

interrupt.o: interrupt.s
	$(CC65)/ca65 --cpu $(CPU) interrupt.s

vectors.o: vectors.s
	$(CC65)/ca65 --cpu $(CPU) vectors.s

crt0.o: crt0.s
	$(CC65)/ca65 --cpu $(CPU) crt0.s

$(LIB): crt0.o supervision.lib
	cp supervision.lib tmp.lib
	$(CC65)/ar65 a tmp.lib crt0.o
	mv tmp.lib $(LIB)
