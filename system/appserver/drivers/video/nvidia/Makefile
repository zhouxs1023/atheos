COPTS   = -I$(TOP)/src  -c -O2 -Wall

DEVBIN = /system/drivers/appserver/video
OBJS =	nvidia.o riva_hw.o

OBJDIR	:= objs
OBJS	:= $(addprefix $(OBJDIR)/,$(OBJS))


#
#	rules
#

$(OBJDIR)/%.o : %.c
	$(CC) $(COPTS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CC) $(COPTS) $< -o $@


all :	objs $(OBJDIR)/nvidia

objs:
	mkdir objs

$(OBJDIR)/nvidia: $(OBJS)
	gcc -plugin $(OBJS) -o $(OBJDIR)/nvidia -latheos -lstdc++

install: $(OBJDIR)/nvidia
	cp -f $(OBJDIR)/nvidia $(DEVBIN)/nvidia
clean:
	-rm $(OBJS) $(OBJDIR)/nvidia
