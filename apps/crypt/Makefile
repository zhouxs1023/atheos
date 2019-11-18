COPTS	= -Wall -c

OBJS	= main.o
EXE	= crypt

CC	= gcc
LL	= gcc

$(EXE) : $(OBJS)
	$(LL) $(OBJS) -lcrypt -o $@

install: $(EXE)
	cp -f $(EXE) /bin/$(EXE)
	strip --strip-all /bin/$(EXE)
clean:
	-rm $(OBJS)
.c.o :
	$(CC) $(COPTS) $< -o $@
.cpp.o:
	$(CC) $(COPTS) $< -o $@

main.o : main.c
	$(CC) $(COPTS) $< -o $@
