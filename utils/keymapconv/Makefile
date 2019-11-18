COPTS	= -Wall -c

OBJS	= keymapconv.o
EXE	= keymapconv

CC	= gcc
LL	= g++

$(EXE) : $(OBJS)
	$(LL) $(OBJS) -latheos -o $@

install: $(EXE)
	cp -f $(EXE) /bin/$(EXE)
	strip --strip-all /bin/$(EXE)
clean:
	-rm $(OBJS)
#.c.o :
#	$(CC) $(COPTS) $< -o $@
.cpp.o:
	$(CC) $(COPTS) $< -o $@

main.o : main.cpp
	$(CC) $(COPTS) $< -o $@
