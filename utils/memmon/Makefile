COPTS	= -Wall -c

OBJS	= main.o
EXE	= memmon

CC	= g++
LL	= g++

$(EXE) : $(OBJS)
	$(LL) $(OBJS) -latheos -o $@

install: $(EXE)
	cp -f $(EXE) /bin/$(EXE)
	strip --strip-all /bin/$(EXE)
clean:
	-rm $(OBJS)
.cpp.o:
	$(CC) $(COPTS) $< -o $@

main.o : main.cpp
	$(CC) $(COPTS) $< -o $@
