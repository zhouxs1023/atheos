CFLAGS   = -Wall -c -g -O3 -pipe
CPPFLAGS = $(CFLAGS)

TRANSDIR = /system/translators

VPATH = ./

CC	= gcc
LL	= ld


JPGOBJS = jpegtrans.o
PNGOBJS = pngtrans.o
GIFOBJS = giftrans.o

OBJS = $(JPGOBJS) $(PNGOBJS) $(GIFOBJS)

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/jpegtrans.so $(OBJDIR)/pngtrans.so $(OBJDIR)/giftrans.so

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/jpegtrans.so: $(JPGOBJS)
	g++ -shared $(JPGOBJS) -latheos -ljpeg -L/usr/lib/ -o $(OBJDIR)/jpegtrans.so

$(OBJDIR)/pngtrans.so: $(PNGOBJS)
	g++ -shared $(PNGOBJS) -latheos -lpng -lz -L/usr/lib/ -o $(OBJDIR)/pngtrans.so

$(OBJDIR)/giftrans.so: $(GIFOBJS)
	g++ -shared $(GIFOBJS) -latheos -lungif -lz -L/usr/lib/ -o $(OBJDIR)/giftrans.so

install: $(OBJDIR)/jpegtrans.so $(OBJDIR)/pngtrans.so $(OBJDIR)/giftrans.so
	install -s $(OBJDIR)/jpegtrans.so $(OBJDIR)/pngtrans.so $(OBJDIR)/giftrans.so $(TRANSDIR)/


deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJS) $(OBJDIR)/*.d

-include $(OBJDIR)/*.d
