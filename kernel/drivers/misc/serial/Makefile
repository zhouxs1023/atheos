
CFLAGS   = -kernel -fno-PIC -c -O3 -Wall
AFLAGS   = -c


OBJS := serial.o


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




all :	objs $(OBJDIR)/serial

objs:
	mkdir objs

$(OBJDIR)/serial: $(OBJS) Makefile
	gcc -kernel $(OBJS) -o $(OBJDIR)/serial

install: all
	cp -f $(OBJDIR)/serial /system/drivers/dev/misc/serial


clean:
	-rm $(OBJS)

