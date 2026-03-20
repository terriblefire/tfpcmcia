CC = /opt/amiga/bin/m68k-amigaos-gcc
AS = /opt/amiga/bin/vasmm68k_mot

CFLAGS = -Os -m68000 -Wall -Werror -fomit-frame-pointer -mregparm=4 \
         -fno-builtin -noixemul
LDFLAGS = -nostartfiles -nostdlib -s -noixemul
VASFLAGS = -Fbin -no-opt

OBJS = deviceheader.o device.o iotask.o spi.o scsi.o lseg.o rdb.o debug.o mulsi3.o

all: tfpcmcia.device tfpcmcia.rom

tfpcmcia.device: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc

tfpcmcia.rom: tfpcmcia_rom.s tfpcmcia.device std_lz4.bin
	$(AS) $(VASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

mulsi3.o: mulsi3.S
	$(CC) -m68000 -noixemul -c -o $@ $<

clean:
	rm -f $(OBJS) tfpcmcia.device tfpcmcia.rom

.PHONY: all clean
