
CFLAGS   = -kernel -D__LOADABLE__ -fno-PIC -c -O3 -g -Wall
AFLAGS   = -c


OBJS := afs.o balloc.o afs_inode.o attribs.o index.o journal.o stream.o btree.o bt_delete.o


OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))
FS = afs

#
#	rules
#

$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.s
	$(CC) $(AFLAGS) -x assembler-with-cpp $< -o $@

$(OBJDIR)/%.o : %.S
	$(CC) $(CFLAGS) -x assembler-with-cpp $< -o $@




all : objs $(OBJDIR)/$(FS)

objs:
	mkdir objs

$(OBJDIR)/$(FS): $(OBJS) Makefile
	gcc -kernel $(OBJS) -o $(OBJDIR)/$(FS)

install: all
	cp -f $(OBJDIR)/$(FS) /system/drivers/fs/$(FS)


clean:
	-rm $(OBJS)

