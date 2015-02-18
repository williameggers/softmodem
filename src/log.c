#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sm.h"

FILE *logfile;
int loglevel;

#define LOG_FATAL 0
#define LOG_WARN 1
#define LOG_INFO 2
#define LOG_DEBUG 3

static void logit(int, const char *, va_list);

void
log_init(int level)
{
    char name[128];

    snprintf(name, sizeof(name), "%s.log", sm.name);
    logfile = fopen(name, "a+");
    if (!logfile) {
	printf("cannot open log file %s!\n", name);
	exit(1);
    }
    loglevel = level;
}

void
log_tmp(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(logfile, msg, ap);
    fprintf(logfile, "\n");
    fflush(logfile);
    va_end(ap);
}
void
log_debug(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    logit(LOG_DEBUG, msg, ap);
    va_end(ap);
}
void
log_info(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    logit(LOG_INFO, msg, ap);
    va_end(ap);
}
void
log_warn(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    logit(LOG_WARN, msg, ap);
    va_end(ap);
}
void
fatal(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    logit(LOG_FATAL, msg, ap);
    va_end(ap);
    
    exit(1);
}

void
log_ttytofile(const char *tty)
{
    char file[128];
    FILE *f;

    snprintf(file, sizeof(file), "%s.tty", sm.name);
    f = fopen(file, "w+");
    if (!f)
	fatal("cannot create file %s to write tty name", file);
    fprintf(f, tty);
    fclose(f);
}

static void
logit(int level, const char *msg, va_list ap)
{
    time_t clock;

    if (level <= loglevel) {
	time(&clock);
	fprintf(logfile, "%d ", (int)clock);
	vfprintf(logfile, msg, ap);
	fprintf(logfile, "\n");
	fflush(logfile);
    }
}
