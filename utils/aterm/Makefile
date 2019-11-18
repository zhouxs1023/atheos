COPTS	= -c -O3

OBJS	= main.o tview.o
EXE	= aterm

CC	= gcc
LL	= gcc

aterm.new: $(OBJS)
	$(CC) $(OBJS) -latheos -lstdc++ -o aterm.new

install: aterm.new
	cp -f aterm.new /bin/aterm
	strip --strip-all /bin/aterm

clean:
	-rm $(OBJS)

%.o : %.c
	$(CC) $(COPTS) $< -o $@

%.o : %.cpp
	$(CC) $(COPTS) $< -o $@

main.o:		main.cpp tview.h
tview.o:	tview.cpp tview.h
