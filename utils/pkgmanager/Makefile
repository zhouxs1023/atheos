CFLAGS   = -pipe -O2 -c -Wall
CPPFLAGS= $(CFLAGS)

EXE = pkgmanager

CC	= g++
LL	= g++

OBJS = pkgmanager.o

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR)/$(EXE) $(OBJDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/$(EXE): $(OBJS) Makefile
	g++ $(OBJS) -o $(OBJDIR)/$(EXE) -latheos

install: $(OBJDIR)/$(EXE)
	cp -af $(OBJDIR)/$(EXE) /bin/$(EXE)

deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJS) $(OBJDIR)/*.d $(OBJDIR)/$(EXE)

-include $(OBJDIR)/*.d
