# Makefile for Unix/Xenix lrz and lsz programs
# the makefile is not too well tested yet

CC=gcc
CFLAGS=-O2 -s -m486 -fomit-frame-pointer
BINDIR=/usr/bin
MANDIR=/usr/man/man1

nothing:
	@echo
	@echo "Please study the #ifdef's in crctab.c rbsb.c, lrz.c and lsz.c,"
	@echo "then type 'make system' where system is one of:"
	@echo "	linux	Linux"
	@echo "	sysvr3	SYSTEM 5.3 Unix with mkdir(2)"
	@echo "	sysv	SYSTEM 3/5 Unix"
	@echo "	xenix	Xenix"
	@echo "	x386	386 Xenix"
	@echo "	bsd	Berkeley 4.x BSD, Ultrix, V7"
	@echo "	clean	Clean up your mess"
	@echo

usenet:
	shar -f /tmp/rzsz README Makefile zmodem.h zm.c sz.c rz.c rbsb.c \
	 init.com crc.c vmodem.h vvmodem.c vrzsz.c crctab.c minirb.c \
	 *.1 gz ptest.sh *.t

shar:
	shar -f /tmp/rzsz -m 2000000 README Makefile zmodem.h zm.c \
	 init.com vmodem.h vvmodem.c vrzsz.c sz.c rz.c crctab.c \
	 crc.c rbsb.c minirb.c *.1 gz ptest.sh *.t

unixforum: shar
	compress -b12 /tmp/rzsz.sh

unix:
	undos README zmodem.h zm.c sz.c rz.c \
	 vmodem.h vvmodem.c vrzsz.c crctab.c *.1 \
	 init.com crc.c *.t 

dos:
	todos README zmodem.h zm.c sz.c rz.c \
	 vmodem.h vvmodem.c vrzsz.c crctab.c *.1 \
	 init.com crc.c *.t 
arc:
	rm -f /tmp/rzsz.arc
	arc a /tmp/rzsz README Makefile zmodem.h zm.c sz.c rz.c \
	 vmodem.h vvmodem.c vrzsz.c crctab.c rbsb.c \
	 init.com crc.c *.1 gz ptest.sh *.t minirb.c
	chmod og-w /tmp/rzsz.arc
	mv /tmp/rzsz.arc /t/yam

zoo:
	rm -f /tmp/rzsz.zoo
	zoo a /tmp/rzsz README Makefile zmodem.h zm.c sz.c rz.c \
	 vmodem.h vvmodem.c vrzsz.c crctab.c rbsb.c *.1 \
	 init.com crc.c gz ptest.sh *.t minirb.c
	chmod og-w /tmp/rzsz.zoo
	mv /tmp/rzsz.zoo /t/yam

tags:
	ctags sz.c rz.c zm.c rbsb.c

linux:
	$(CC) $(CFLAGS) -DLINUX -DMD=2 lrz.c -o lrz
	$(CC) $(CFLAGS) -DLINUX -DTXBSIZE=32768 -DNFGVMIN lsz.c -o lsz

xenix:
	cc -M0 -Ox -K -i -DTXBSIZE=16384 -DNFGVMIN -DREADCHECK sz.c -lx -o sz
	size sz
	-ln sz sb
	-ln sz sx
	cc -M0 -Ox -K -i -DMD rz.c -o rz
	size rz
	-ln rz rb
	-ln rz rx

x386:
	cc -Ox -DMD rz.c -o rz
	size rz
	-ln rz rb
	-ln rz rx
	cc -Ox -DTXBSIZE=32768 -DNFGVMIN -DREADCHECK sz.c -lx -o sz
	size sz
	-ln sz sb
	-ln sz sx

sysv:
	cc -O -DMD rz.c -o rz
	size rz
	-ln rz rb
	-ln rz rx
	cc -DSV -O -DTXBSIZE=32768 -DNFGVMIN sz.c -o sz
	size sz
	-ln sz sb
	-ln sz sx

sysvr3:
	cc -O -DMD=2 rz.c -o rz
	size rz
	-ln rz rb
	-ln rz rx
	cc -DSV -O -DTXBSIZE=32768 -DNFGVMIN sz.c -o sz
	size sz
	-ln sz sb
	-ln sz sx

bsd:
	cc -DMD=2 -Dstrchr=index -DV7 -O rz.c -o rz
	size rz
	-ln rz rb
	-ln rz rx
	cc -DV7 -O -DTXBSIZE=32768 -DNFGVMIN sz.c -o sz
	size sz
	-ln sz sb
	-ln sz sx

clean:
	rm -f *.o lrz lsz lrb lsb lrx lsx
