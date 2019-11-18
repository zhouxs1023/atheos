CFLAGS   = -pipe -O2 -c -Wall
CPPFLAGS= $(CFLAGS)

EXE = AInstall

CC	= gcc
LL	= ld

OBJS =	main.o installview.o diskmanager.o

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/$(EXE)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/$(EXE): $(OBJS)
	gcc $(OBJS) -o $(OBJDIR)/$(EXE) -latheos -lstdc++

install: $(OBJDIR)/$(EXE)
	install -s $(OBJDIR)/$(EXE) /bin/$(EXE)

deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJS) $(OBJDIR)/*.d $(OBJDIR)/$(EXE)

-include $(OBJDIR)/*.d
