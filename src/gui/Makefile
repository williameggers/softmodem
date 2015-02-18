OBJS=modem_monitor.o
CFLAGS+=-DHAVE_FL_FL_H -DHAVE_FL_FL_CARTESIAN_H
CFLAGS+=-I/usr/local/include

all: $(OBJS)

modem_monitor.o:
	$(CC) $(CFLAGS) -c modem_monitor.cpp

clean:
	rm -f *~ *.o

