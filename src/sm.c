#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include "sm.h"

extern char *__progname;

void
sighldr(int sig)
{
    log_info("received SIG %d !", sig);
    switch (sig) {
    case SIGQUIT:
    case SIGINT: 
    case SIGTERM:
	sm.on = 0;
	break;
    case SIGUSR1:
	sm.sigrestart = 1;
	break;
    case SIGUSR2:
	sm.sigbreak_local = 1;
	sm.sigbreak_send = 1;
	break;
    }
}

void
droppriv()
{
    struct passwd *pw;
    char smdev[64];

    log_info("dropping privileges to user %s", SM_USER);
    pw = getpwnam(SM_USER);
    if (!pw)
	fatal("unknown user %s", SM_USER);

    snprintf(smdev, sizeof(smdev), "/dev/%s", sm.name);
    chown(smdev, pw->pw_uid, -1);

    if (setgroups(1, &pw->pw_gid) ||
	setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
	fatal("can't drop privileges");
}

void
usage()
{
    printf("usage: %s [-vgcpd] [-n name] [-m modem,option]\n", __progname);
    exit(1);
}

int
main (int argc, char *argv[])
{
    int loglevel = 1;
    int jack_connect_ports = 0;
    int devsm = 0;
    int ch;
    char **ap;

    sm.name = NULL;
    sm.use_gui = 0;
    sm.caller = 0;
    while ((ch = getopt(argc, argv, "vgcpdn:m:h")) != -1) {
	switch (ch) {
	    case 'v':
		loglevel++;
		break;
	    case 'g':
		sm.use_gui = 1;
		break;
	    case 'c':
		sm.caller = 1;
		break;
	    case 'p':
		jack_connect_ports = 1;
		break;
	    case 'd':
		devsm = 1;
		break;
	    case 'n':
		sm.name = strdup(optarg);
#ifdef __OpenBSD__
		setproctitle(optarg);
#endif
		break;
	    case 'm':
		for (ap = sm.modem; ap < &(sm.modem)[5] &&
		    (*ap = strsep(&optarg, ",")) != NULL;) {
		    if (**ap == '\0')
			*ap = NULL;
		    ap++;
		}
		*ap = NULL;
		break;
	    case 'h':
	    default:
		usage();
	}
    }
    if (!sm.name)
	sm.name = strdup(__progname);

    sm.sigrestart = 0;
    sm.sigbreak_local = 0;
    sm.sigbreak_send = 0;
    signal(SIGQUIT, sighldr);
    signal(SIGINT, sighldr);
    signal(SIGTERM, sighldr);
    signal(SIGUSR1, sighldr);
    signal(SIGUSR2, sighldr);

    log_init(loglevel);
    tty_init(devsm);
    if (geteuid() == 0)
	droppriv();
    jack_init(jack_connect_ports);
    if (sm.use_gui) gui_init();
    modem_init();

    sm.on = 1;
    tty_read_loop();

    exit(0);
}

