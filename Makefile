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

CC65_FLAGS = -t none --cpu $(CPU) --register-vars

$(ROM): a.out
	(dd count=5 bs=4096 if=/dev/zero 2> /dev/null; cat a.out) > $(ROM)

run: $(ROM)
	$(APPLE2E) -mute -map main.map $(ROM)

a.out: main.o interrupt.o vectors.o exporter.o platform.o runtime.o apple2rom.cfg $(LIB)
	$(CC65)/ld65 -C apple2rom.cfg -m main.map --dbgfile main.dbg interrupt.o vectors.o exporter.o platform.o runtime.o main.o $(LIB)
	awk -f rom_usage.awk < main.map

clean:
	rm -f *.o *.lst a.out platform.s runtime.s main.s $(LIB) tmp.lib

main.s: main.c exporter.h platform.h runtime.h
	$(CC65)/cc65 $(CC65_FLAGS) -O $<

runtime.s: runtime.c runtime.h
	$(CC65)/cc65 $(CC65_FLAGS) -O $<

%.o: %.s
	$(CC65)/ca65 -l $(<:.s=.lst) --cpu $(CPU) $<

# platform.c contains inline assembly and code that must not be optimized
platform.s: platform.c
	$(CC65)/cc65 $(CC65_FLAGS) $<

platform.o: platform.s
interrupt.o: interrupt.s
vectors.o: vectors.s
exporter.o: exporter.s
crt0.o: crt0.s

$(LIB): crt0.o supervision.lib
	cp supervision.lib tmp.lib
	$(CC65)/ar65 a tmp.lib crt0.o
	mv tmp.lib $(LIB)
