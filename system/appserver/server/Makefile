CFLAGS   = -pipe -O2 -g -I./freetype2-beta8/include -c -Wall
#CFLAGS   = -pipe -O2 -g -I./freetype-2.0.4/include -c -Wall
CPPFLAGS= $(CFLAGS)
AOPTS   = -c

CC	= gcc
LL	= ld

OBJS =	ddriver.o ddline.o vesadrv.o server.o input.o	\
	config.o keyboard.o keymap.o defaultkeymap.o			\
	sapplication.o swindow.o layer.o render.o bitmap.o crdtrans.o	\
	sfont.o fontnode.o wndborder.o desktop.o winselect.o sprite.o	\
	clipboard.o windowdecorator.o defaultdecorator.o

OBJS += screenshot.o
#CFLAGS += -D__NO_SCREENSHOT_SUPPORT__

OBJDIR	:= objs

include $(ATHEOS_SRC)/makefile.rules

all :	$(OBJDIR) $(OBJDIR)/appserver

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/appserver: $(OBJS)
#	gcc $(OBJS) -o $(OBJDIR)/appserver freetype-2.0.4/objs/libfreetype.a -latheos -lpng -lz -lstdc++ -lm
	gcc $(OBJS) -o $(OBJDIR)/appserver freetype2-beta8/obj/libfreetype.a -latheos -lpng -lz -lstdc++ -lm
#	gcc $(OBJS) -o $(OBJDIR)/appserver freetype2-beta8/obj/libfreetype.a -latheos -lstdc++ -lm

$(OBJDIR)/mini_appserver: $(OBJS)
	gcc $(OBJS) -o $(OBJDIR)/appserver freetype2-beta8/obj/libfreetype.a -latheos -lstdc++ -lm

install: $(OBJDIR)/appserver
	cp -f $(OBJDIR)/appserver /system/appserver

deps: $(OBJDIR) $(DEPS)

clean:
	rm -f $(OBJS) $(OBJDIR)/*.d $(OBJDIR)/appserver

-include $(OBJDIR)/*.d
