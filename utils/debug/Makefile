COPTS	= -c -Wall

OBJS	= term.o
EXE	= dbterm

CC	= gcc
LL	= gcc

all: dbterm
# rfs

$(EXE) : $(OBJS)
	$(LL) $(OBJS)  -o $(EXE)
#rfs:	rfs.o
#	$(LL) rfs.o -o rfs

ftp:	ftp.o
	$(LL) ftp.o -o ftp

.cpp.o :
	$(CC) $(COPTS) $< -o $@
.c.o :
	$(CC) $(COPTS) $< -o $@

install:
	cp -f $(EXE) /bin/$(EXE)
#	cp -f rfs /bin/rfs

term.o : term.c

clean:
	-rm $(OBJS) dbterm rfs
