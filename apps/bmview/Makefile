CFLAGS	= -Wall -c -I/usr/libungif/include

OBJS	= main.o
EXE	= bmview

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
#	$(CC) $(CFLAGS) $< -o $@
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

main.o : main.cpp
	$(CC) $(CFLAGS) $< -o $@
