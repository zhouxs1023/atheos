COPTS	= -O3 -Wall -c

OBJS	= main.o fontpanel.o screenpanel.o windowpanel.o keyboardpanel.o coloredit.o
EXE	= guiprefs

CC	= gcc
LL	= g++
LIBS	= -lm -latheos

$(EXE) : $(OBJS)
	$(LL) $(OBJS) $(LIBS) -o $@

install: $(EXE)
	cp -f $(EXE) /bin/$(EXE)
clean:
	-rm $(OBJS)
#.c.o :
#	$(CC) $(COPTS) $< -o $@
.cpp.o:
	$(CC) $(COPTS) $< -o $@

main.o : main.cpp
	$(CC) $(COPTS) $< -o $@
