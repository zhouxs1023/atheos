CFLAGS	 = -Wall -c
CPPFLAGS = -Wall -c

OBJS	= main.o editview.o controlpanel.o iconview.o winicon.o bitmapscale.o
EXE	= iconedit

CC	= gcc
LL	= gcc

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

$(OBJDIR)/$(EXE) : $(OBJDIR) $(OBJS)
	$(LL) $(OBJS) -latheos -lstdc++ -lm -o $@

$(OBJDIR):
	mkdir $(OBJDIR)

deps: $(OBJDIR) $(DEPS)
install: $(OBJDIR)/$(EXE)
	cp -f $(OBJDIR)/$(EXE) /bin/$(EXE)
	strip --strip-all /bin/$(EXE)
clean:
	-rm $(OBJS) $(OBJDIR)/*.d $(OBJDIR)/$(EXE)

-include $(OBJDIR)/*.d
#.c.o :
#	$(CC) $(COPTS) $< -o $@
#.cpp.o:
#	$(CC) $(COPTS) $< -o $@


#main.o:		main.cpp controlid.h
#editview.o:	editview.cpp editview.h controlid.h
#iconview.o:	iconview.cpp iconview.h controlid.h
#winicon.o:	winicon.cpp controlid.h
#bitmapscale.o:	bitmapscale.cpp bitmapscale.h controlid.h
#controlpanel.o:	controlpanel.cpp controlpanel.h controlid.h
