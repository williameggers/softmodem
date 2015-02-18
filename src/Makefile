PROG = ../bin/sm
OBJS = sm.o jack.o tty.o modem.o log.o gui.o
CFLAGS+=-Wall -I/usr/local/include
LDFLAGS=-ljack -lpthread -lutil -lspandsp -L/usr/local/lib

all:
	make $(OBJS)
	$(CC) $(OBJS) -o $(PROG) $(LDFLAGS)

withgui:
	cd gui; $(MAKE) all; cd ..
	CFLAGS="$(CFLAGS) -DENABLE_GUI" make $(OBJS)
	$(CC) $(OBJS) -o $(PROG) $(LDFLAGS) gui/modem_monitor.o -lfltk -lfltk_cartesian -lfltk_audio_meter -L/usr/X11R6/lib -lX11

.c.o: 
	$(CC) $(CFLAGS) -c $<

clean:
	cd gui; $(MAKE) clean; cd ..
	rm -f $(PROG) *~ *.o

