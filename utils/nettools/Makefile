COPTS	= -Wall -c -fPIC

#OBJS	= main.o
#EXE	= netstat

CC	= gcc
LL	= gcc

#$(EXE) : $(OBJS)
#	$(LL) $(OBJS) -o $@

all: route ifconfig

ifconfig: ifconfig.o
	$(LL) ifconfig.o -o $@

route: route.o
	$(LL) route.o -o $@

install: $(EXE)
	cp -f ifconfig /bin/ifconfig
	cp -f route /bin/route
clean:
	-rm ifconfig.o route.o
#.c.o :
#	$(CC) $(COPTS) $< -o $@
.cpp.o:
	$(CC) $(COPTS) $< -o $@

main.o : main.cpp
	$(CC) $(COPTS) $< -o $@
