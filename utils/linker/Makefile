COPTS	= -c -O2
LOPTS	=

STARTUP	= stdinit.o
LNK_OBJS= ald.o coff.o dll.o aelf.o
LD_OBJS	= ld.o

CC	= gcc
LL	= gcc

#SUFFIXES = .d
#%.exe : %
#	echo make exe -----------------------------> $< -o $@

all:	ld alnk

alnk:	$(LNK_OBJS)
	$(LL) $(LNK_OBJS) $(LOPTS) -o alnk

ld:	$(LD_OBJS)
	$(LL) $(LD_OBJS) $(LOPTS) -o ld

.c.o :
	$(CC) $(COPTS) $< -o $@

ald.o:	ald.c
coff.o:	coff.c
dll.o:	dll.c
aelf.o:	aelf.c

ld.o:	ld.c

install: all
	cp -f alnk ../../../sys/bin/alnk
	cp -f ld  ../../../sys/bin/ld

restore:
	cp -f ../../sys/bin/alnk.bak ../../sys/bin/alnk
