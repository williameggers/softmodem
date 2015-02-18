#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#ifndef __OpenBSD__
# ifndef USE_UNIX98_PTY
#  include <pty.h>
# endif
#else
# include <util.h>
#endif
#include "sm.h"

int master;
int slave;

void
tty_init(int devsm)
{
    int flags;
    struct termios tm;
    static char name[20];

    if (openpty(&master, &slave, name, NULL, NULL))
	fatal("tty_init: cannot openpty");

    /* XXX learn termios functions */
    if (tcgetattr(slave, &tm) == 0) {
	tm.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tm.c_cflag |= CS8 | CREAD; //CLOCAL ?
	tm.c_iflag = IGNPAR;
	tm.c_oflag = 0;
	tm.c_lflag = 0;
	if (tcsetattr(slave, TCSAFLUSH, &tm) < 0)
	    fatal("tty_init: cannot set terminal attributes");
    }

    if ((flags = fcntl(master, F_GETFL)) != -1)
	if (fcntl(master, F_SETFL, flags | O_NONBLOCK) == -1)
	    fatal("tty_init: cannot set terminal nonblock");

    log_info("tty_init: %s opened", name);

    if (devsm) {
	char smname[64];

	snprintf(smname, sizeof(smname), "/dev/%s", sm.name);
	if (!unlink(smname))
	    log_info("tty_init: removed old %s", smname);
	if (symlink(name, smname) < 0)
	    fatal("tty_init: could not create symlink %s -> %s", smname, name);
    } else
	log_ttytofile(name);
}

void
tty_write(uint8_t *buf, int len)
{
    ssize_t wrote;

    log_debug("tty_write: [%d]", len);
    wrote = write(master, buf, len);
    if (wrote == -1) {
	log_warn("tty_write: error writing a %d bytes buffer", len);
    } else if (wrote != len) {
	log_warn("tty_write: unable to write full buf of %d bytes, %d written",
		len, (int)wrote);
	if (tcflush(master, TCOFLUSH))
	    log_warn("tty_write: unable to flush master");
	else if (tcflush(slave, TCOFLUSH))
	    log_warn("tty_write: unable to flush slave");
    }
}

void
tty_read_loop()
{
    fd_set fdsr;
    int nfds, fdret;
    int readlen, free, toread;
    uint8_t readbuf[1024];

    do
    {
	FD_ZERO(&fdsr);
	FD_SET(master, &fdsr);
	nfds = master + 1;
	log_debug("tty_read_loop: going select");
	fdret = select(nfds, &fdsr, NULL, NULL, NULL);
	log_debug("tty_read_loop: select done");
	if (fdret > 0 && FD_ISSET(master, &fdsr)) {
	    free = modem_modqueue_isready();
	    if (free > 0) {
		toread = (free > sizeof(readbuf) ? sizeof(readbuf) : free);
		log_debug("tty_read_loop: going read %d bytes", toread);
		readlen = read(master, readbuf, toread);
		log_debug("tty_read_loop: read done %d bytes", readlen);
		if (readlen > 0)
		    modem_modqueue_append(readbuf, readlen);
	    } else {
		log_debug("tty_read_loop: sleeping 1s waiting for modqueue");
		sleep(1);
	    }
	}
    } while (sm.on);

    log_info("tty_read_loop: exiting");
}
