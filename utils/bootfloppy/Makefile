CFLAGS	= -Wall -c -O2


CC	= gcc
LL	= g++

OBJDIR=objs

all: $(OBJDIR) $(OBJDIR)/makeramdisk $(OBJDIR)/splitramdisk $(OBJDIR)/rdloader


$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/rdloader : $(OBJDIR)/rdloader.o
	$(LL) $(OBJDIR)/rdloader.o -latheos -lz -o $@

$(OBJDIR)/makeramdisk: $(OBJDIR)/makeramdisk.o
	$(LL) $(OBJDIR)/makeramdisk.o -o $@

$(OBJDIR)/splitramdisk: $(OBJDIR)/splitramdisk.o
	$(LL) $(OBJDIR)/splitramdisk.o -o $@

install: $(OBJDIR)/makeramdisk $(OBJDIR)/splitramdisk
	install -s $(OBJDIR)/makeramdisk /bin/makeramdisk
	install -s $(OBJDIR)/splitramdisk /bin/splitramdisk

clean:
	-rm $(OBJS)

$(OBJDIR)/%.o : %.c
	@echo Compiling : $<
	@$(CC) $(CFLAGS) $< -o $@
$(OBJDIR)/%.o : %.cpp
	@echo Compiling : $<
	@$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/rdloader.o:		rdloader.cpp
$(OBJDIR)/makeramdisk.o:	makeramdisk.c
$(OBJDIR)/splitramdisk.o:	splitramdisk.c
