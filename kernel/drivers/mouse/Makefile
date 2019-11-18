
CFLAGS   = -D__KERNEL__ -fno-PIC -c -O3 -Wall
AFLAGS   = -c


OBJS := mouse.o


OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))


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




all :	objs $(OBJDIR)/mouse

objs:
	mkdir objs

$(OBJDIR)/mouse: $(OBJS) Makefile
	gcc -nostdlib $(OBJS) -lgcc -lkernel -o $(OBJDIR)/mouse

install: all
	cp -f $(OBJDIR)/mouse /system/drivers/dev/mouse


clean:
	-rm $(OBJS)

