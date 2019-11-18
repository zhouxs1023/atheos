CFLAGS   = -D__KERNEL__ -fno-PIC -c -O3 -Wall

OBJS = nfs.o


#
#	rules
#

$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CC) $(CPPFLAGS) $< -o $@

$(OBJDIR)/%.o : %.s
	$(CC) $(AFLAGS) -x assembler-with-cpp $< -o $@

$(OBJDIR)/%.o : %.S
	$(CC) $(CFLAGS) -x assembler-with-cpp $< -o $@

all: nfs

nfs: nfs.o Makefile
	gcc --fno-PIC -nostdlib nfs.o -L/system/ -lkernel -o nfs

install: all
	cp -f nfs /system/drivers/fs/nfs


nfs.o: nfs.c


