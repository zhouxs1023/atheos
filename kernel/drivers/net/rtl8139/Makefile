
CFLAGS   = -kernel -fno-PIC -c -O3
# -Wall

C_INCLUDE_PATH=/ainc

OBJS := rtl8139.o


OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))

$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CC) $(CFLAGS) $< -o $@


$(OBJDIR)/%.o : %.s
	$(CC) $(CFLAGS) -x assembler-with-cpp $< -o $@

$(OBJDIR)/%.o : %.S
	$(CC) $(CFLAGS) -x assembler-with-cpp $< -o $@


all :	objs $(OBJDIR)/rtl8139

objs:
	mkdir objs

$(OBJDIR)/rtl8139: $(OBJS) Makefile
	gcc -kernel $(OBJS) -o $(OBJDIR)/rtl8139

install: all
	cp -f $(OBJDIR)/rtl8139 /system/drivers/dev/net/eth/rtl8139


clean:
	-rm $(OBJS)

