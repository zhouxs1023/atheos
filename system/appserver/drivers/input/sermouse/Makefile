COPTS   = -I../../../server  -c -O2 -Wall

DEVBIN = /system/drivers/appserver/input
OBJS =	sermouse.o

OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))


#
#	rules
#

$(OBJDIR)/%.o : %.c
	$(CC) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CC) $(COPTS) $< -o $@


all :	objs $(OBJDIR)/sermouse

objs:
	mkdir objs

$(OBJDIR)/sermouse: $(OBJS)
	gcc -plugin $(OBJS) -o $(OBJDIR)/sermouse -latheos -lstdc++

install: $(OBJDIR)/sermouse
	cp -f $(OBJDIR)/sermouse $(DEVBIN)/sermouse
clean:
	-rm $(OBJS) $(OBJDIR)/sermouse
