COPTS	= -Wall -c -I../../kernel/drivers/misc/w8378x/

OBJS	= main.o graphview.o barview.o
EXE	= cpu

CC	= g++
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
barview.o : barview.cpp barview.h
	$(CC) $(COPTS) $< -o $@
graphview.o : graphview.cpp graphview.h
	$(CC) $(COPTS) $< -o $@
